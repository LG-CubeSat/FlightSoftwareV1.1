#include "imu.h"
#include "main.h"
#include "platform/adcs_environment.h"
#include "bmi270_config.h"
#include <stdio.h>

extern I2C_HandleTypeDef hi2c2;

/*
 * I2C 7-bit addresses shifted left by 1 for STM32 HAL:
 * 0x68 << 1 = 0xD0 (SDO pulled low / GND)
 * 0x69 << 1 = 0xD2 (SDO pulled high / 3.3V)
 */
#define BMI270_I2C_ADDR_LOW        (0x68 << 1)
#define BMI270_I2C_ADDR_HIGH       (0x69 << 1)

/* --- BMI270 Register Map --- */
#define BMI270_REG_CHIP_ID         0x00
#define BMI270_REG_DATA_START      0x0C  /* DATA_8 (ACC_X_LSB) to DATA_19 (GYR_Z_MSB) */
#define BMI270_REG_INTERNAL_STATUS 0x21
#define BMI270_REG_TEMP_LSB        0x22
#define BMI270_REG_ACC_CONF        0x40
#define BMI270_REG_ACC_RANGE       0x41
#define BMI270_REG_GYR_CONF        0x42
#define BMI270_REG_GYR_RANGE       0x43
#define BMI270_REG_INIT_CTRL       0x59
#define BMI270_REG_INIT_ADDR_0     0x5B
#define BMI270_REG_INIT_ADDR_1     0x5C
#define BMI270_REG_INIT_DATA       0x5E
#define BMI270_REG_PWR_CONF        0x7C
#define BMI270_REG_PWR_CTRL        0x7D
#define BMI270_REG_CMD             0x7E

#define BMI270_CHIP_ID_VAL         0x24
#define BMI270_CMD_SOFT_RESET      0xB6
#define BMI270_CHUNK_SIZE          32

static uint16_t s_bmi_addr = BMI270_I2C_ADDR_LOW;

imu_status_t imu_initialize(void) {
    uint8_t chip_id = 0;
    uint8_t val = 0;

    /* 1. Probe I2C address & verify CHIP_ID (0x24) */
    s_bmi_addr = BMI270_I2C_ADDR_LOW;
    if (HAL_I2C_Mem_Read(&hi2c2, s_bmi_addr, BMI270_REG_CHIP_ID, 1, &chip_id, 1, 100) != HAL_OK || chip_id != BMI270_CHIP_ID_VAL) {
        s_bmi_addr = BMI270_I2C_ADDR_HIGH;
        if (HAL_I2C_Mem_Read(&hi2c2, s_bmi_addr, BMI270_REG_CHIP_ID, 1, &chip_id, 1, 100) != HAL_OK || chip_id != BMI270_CHIP_ID_VAL) {
            return IMU_NOT_READY;
        }
    }

    /* 2. Soft reset the device */
    val = BMI270_CMD_SOFT_RESET;
    HAL_I2C_Mem_Write(&hi2c2, s_bmi_addr, BMI270_REG_CMD, 1, &val, 1, 100);
    HAL_Delay(2); /* Minimum 2 ms after reset */

    /* 3. Disable advanced power save mode so internal clocks run */
    val = 0x00;
    if (HAL_I2C_Mem_Write(&hi2c2, s_bmi_addr, BMI270_REG_PWR_CONF, 1, &val, 1, 100) != HAL_OK) {
        return IMU_ERROR;
    }
    HAL_Delay(1); /* Allow internal oscillator to stabilize (> 450 us) */

    /* 4. Prepare for configuration file upload */
    val = 0x00;
    if (HAL_I2C_Mem_Write(&hi2c2, s_bmi_addr, BMI270_REG_INIT_CTRL, 1, &val, 1, 100) != HAL_OK) {
        return IMU_ERROR;
    }

    /* 5. Stream the 8192-byte configuration blob in 32-byte chunks */
    for (uint16_t i = 0; i < sizeof(bmi270_config_file); i += BMI270_CHUNK_SIZE) {
        uint16_t word_addr = i / 2;
        uint8_t addr_bytes[2] = {
            (uint8_t)(word_addr & 0x0F),         /* INIT_ADDR_0 [3:0] */
            (uint8_t)((word_addr >> 4) & 0xFF)   /* INIT_ADDR_1 [11:4] */
        };

        /* Set word address */
        if (HAL_I2C_Mem_Write(&hi2c2, s_bmi_addr, BMI270_REG_INIT_ADDR_0, 1, addr_bytes, 2, 100) != HAL_OK) {
            return IMU_ERROR;
        }

        /* Burst write 32 bytes into INIT_DATA */
        if (HAL_I2C_Mem_Write(&hi2c2, s_bmi_addr, BMI270_REG_INIT_DATA, 1,
                              (uint8_t *)&bmi270_config_file[i], BMI270_CHUNK_SIZE, 100) != HAL_OK) {
            return IMU_ERROR;
        }
    }

    /* 6. Complete configuration upload */
    val = 0x01;
    if (HAL_I2C_Mem_Write(&hi2c2, s_bmi_addr, BMI270_REG_INIT_CTRL, 1, &val, 1, 100) != HAL_OK) {
        return IMU_ERROR;
    }
    HAL_Delay(20); /* Wait 20 ms for ASIC initialization and CRC verification */

    /* 7. Verify initialization status in register 0x21 */
    uint8_t init_status = 0;
    if (HAL_I2C_Mem_Read(&hi2c2, s_bmi_addr, BMI270_REG_INTERNAL_STATUS, 1, &init_status, 1, 100) != HAL_OK ||
        (init_status & 0x01) != 0x01) {
        return IMU_ERROR; /* ASIC failed to initialize */
    }

    /* 8. Configure ranges and filters */
    val = 0xA8; /* Accel: 100 Hz ODR, Normal filter */
    HAL_I2C_Mem_Write(&hi2c2, s_bmi_addr, BMI270_REG_ACC_CONF, 1, &val, 1, 100);

    val = 0x00; /* Accel: +/- 2g range */
    HAL_I2C_Mem_Write(&hi2c2, s_bmi_addr, BMI270_REG_ACC_RANGE, 1, &val, 1, 100);

    val = 0xE8; /* Gyro: 100 Hz ODR, High-Performance noise & filter */
    HAL_I2C_Mem_Write(&hi2c2, s_bmi_addr, BMI270_REG_GYR_CONF, 1, &val, 1, 100);

    val = 0x00; /* Gyro: +/- 2000 deg/s range */
    HAL_I2C_Mem_Write(&hi2c2, s_bmi_addr, BMI270_REG_GYR_RANGE, 1, &val, 1, 100);

    /* 9. Power on Accelerometer, Gyroscope, and Temperature */
    val = 0x0E; /* acc_en (bit 2) | gyr_en (bit 1) | temp_en (bit 3) */
    if (HAL_I2C_Mem_Write(&hi2c2, s_bmi_addr, BMI270_REG_PWR_CTRL, 1, &val, 1, 100) != HAL_OK) {
        return IMU_ERROR;
    }

    return IMU_OK;
}

imu_status_t imu_read(imu_sample_t *sample) {
    if (sample == NULL) return IMU_ERROR;

    uint8_t raw[12];

    /* Burst read 12 bytes: Accel (6B) + Gyro (6B) from 0x0C */
    if (HAL_I2C_Mem_Read(&hi2c2, s_bmi_addr, BMI270_REG_DATA_START, 1, raw, 12, 50) != HAL_OK) {
        return IMU_ERROR;
    }

    sample->timestamp_us = adcs_monotonic_time_us();

    /* Unpack Little-Endian signed 16-bit counts */
    int16_t ax = (int16_t)((raw[1] << 8) | raw[0]);
    int16_t ay = (int16_t)((raw[3] << 8) | raw[2]);
    int16_t az = (int16_t)((raw[5] << 8) | raw[4]);

    int16_t gx = (int16_t)((raw[7] << 8) | raw[6]);
    int16_t gy = (int16_t)((raw[9] << 8) | raw[8]);
    int16_t gz = (int16_t)((raw[11] << 8) | raw[10]);

    /* Accel conversion (+/- 2g range: 16384 LSB/g) -> m/s^2 */
    const float accel_scale = 9.80665F / 16384.0F;
    sample->acceleration_m_s2[0] = (float)ax * accel_scale;
    sample->acceleration_m_s2[1] = (float)ay * accel_scale;
    sample->acceleration_m_s2[2] = (float)az * accel_scale;

    /* Gyro conversion (+/- 2000 dps range: 16.384 LSB/(deg/s)) -> rad/s */
    const float gyro_scale = (2000.0F / 32768.0F) * (3.1415926535F / 180.0F);
    sample->angular_rate_rad_s[0] = (float)gx * gyro_scale;
    sample->angular_rate_rad_s[1] = (float)gy * gyro_scale;
    sample->angular_rate_rad_s[2] = (float)gz * gyro_scale;

    /* Read Temperature (registers 0x22 / 0x23) */
    uint8_t temp_raw[2];
    if (HAL_I2C_Mem_Read(&hi2c2, s_bmi_addr, BMI270_REG_TEMP_LSB, 1, temp_raw, 2, 20) == HAL_OK) {
        int16_t raw_temp = (int16_t)((temp_raw[1] << 8) | temp_raw[0]);
        sample->temperature_c = ((float)raw_temp / 512.0F) + 23.0F;
    }

    return IMU_OK;
}
