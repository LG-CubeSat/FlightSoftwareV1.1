#include "imu.h"
#include "main.h"
#include "platform/adcs_environment.h"
#include <stdio.h>

extern I2C_HandleTypeDef hi2c2;

#define BMI270_I2C_ADDR_LOW   (0x68 << 1) /* AD0 pulled low (GND) */
#define BMI270_I2C_ADDR_HIGH  (0x69 << 1) /* AD0 pulled high (3.3V) */

#define BMI270_REG_CHIP_ID 0x00 /* Read-only chip identity */
#define BMI270_REG_DATA_START 0x0C /* Start of 12-byte sensor data (DATA_8) */
#define BMI270_REG_INTERNAL_STATUS 0x21 /* ASIC Boot status and error */
#define BMI270_REG_ACC_CONF 0x40 /* Accelerometer ODR and bandwidth filter */
#define BMI270_REG_ACC_RANGE 0x41 /* Accelerometer full-scale range (+/- 2g to 16g) */
#define BMI270_REG_GYR_CONF 0x42 /* Gyroscope ODR & filter performance */
#define BMI270_REG_GYR_RANGE 0x43 /* Gyroscope full-scale range (+/- 125 to 2000 dps) */
#define BMI270_REG_INIT_CTRL 0x59 /* Control config upload sequence */
#define BMI270_REG_INIT_ADDR_0 0x5B /* Word address lower nibble [3:0] */
#define BMI270_REG_INIT_ADDR_1 0x5C /* Word address upper byte [11:4] */
#define BMI270_REG_INIT_DATA 0x5E /* Burst write window for firmware blob */
#define BMI270_REG_PWR_CONF 0x7C /* Power mode & advanced power save */
#define BMI270_REG_PWR_CTRL 0x7D /* Subsystem power switch (acc, gyr, temp) */
#define BMI270_REG_CMD 0x7E /* Command register (soft reset) */

#define BMI270_CHIP_ID_VAL 0x24 /* Expected hardware ID for BMI270 */
#define BMI270_CMD_SOFT_RESET 0xB6 /* Magic byte to trigger POR reset */
#define BMI270_CHUNK_SIZE 32 /* Safe I2C burst payload size (bytes) */

static uint16_t s_bmi_addr = BMI270_I2C_ADDR_LOW;

imu_status_t imu_initialize(void) {
    uint8_t chip_id = 0;
    uint8_t val = 0;

    // check address 0x68 first. If not found or ID doesn't match 0x24 check 0x69
    s_bmi_addr = BMI270_I2C_ADDR_LOW;
    if (HAL_I2C_Mem_Read(&hi2c2, s_bmi_addr, BMI270_REG_CHIP_ID, 1, &chip_id, 1, 100) != HAL_OK || chip_id != BMI270_CHIP_ID_VAL) {
        s_bmi_addr = BMI270_I2C_ADDR_HIGH;
        if (HAL_I2C_Mem_Read(&hi2c2, s_bmi_addr, BMI270_REG_CHIP_ID, 1, &chip_id, 1, 100) != HAL_OK || chip_id != BMI270_CHIP_ID_VAL) {
            return IMU_NOT_READY;
        }
    }

    // issue soft-reset command. All registers to default
    val = BMI270_CMD_SOFT_RESET;
    HAL_I2C_Mem_Write(&hi2c2, s_bmi_addr, BMI270_REG_CMD, 1, &val, 1, 100);
    HAL_Delay(2); // Wait 2 ms (datasheet requires >= 1.5 ms for POR)

    // Disable Advanced Power Save Mode
    val = 0x00;
    if (HAL_I2C_MemWrite(&hi2c2, s_bmi_addr, BMI270_REG_PWR_CONF, 1, &val, 1, 100) != HAL_OK) {
        return IMU_ERROR;
    }
    HAL_Delay(1); // wait 1 ms (datasheet requires >= 450 us for clocks to stabalize)

    val = 0x00;
    if (HAL_I2C_Mem_Write(&hi2c2, s_bmi_addr, BMI270_REG_INIT_CTRL, 1, &val, 1, 100) != HAL_OK) {
        return IMU_ERROR;
    }

    // Burst-write the 8192-byte array in 32-byte chunks
    for (uint16_t i = 0; i < sizeof(bmi270_config_file); i += BMI270_CHUNK_SIZE) {
        // first BMI memory is addressed in 16-bit words (2 bytes per word)
        uint16_t word_addr = i / 2;

        // prepare the 12 bit word address across 2 registers (0x5B and 0x5C)
        uint8_t addr_bytes[2];
        addr_bytes[0] = (uint8_t)(word_addr & 0x0F);
        addr_bytes[1] = (uint8_t)((word_addr >> 4) & 0xFF); // INIT_ADDR_1 takes bits [11:4]

        // Set the start address for this chunk
        if (HAL_I2C_Mem_Write(&hi2c2, s_bmi_addr, BMI270_REG_INIT_ADDR_0, 1, addr_bytes, 2, 100) != HAL_OK) {
            return IMU_ERROR;
        }

        // Stream the 32 bytes into the auto-incrementing data register (0x5E)
        if (HAL_I2C_Mem_Write(&hi2c2, s_bmi_addr, BMI270_REG_INIT_DATA, 1, 
                (uint8_t *)&bmi270_config_file[i], BMI270_CHUNK_SIZE, 100) != HAL_OK) {
            return IMU_ERROR;
        }

        // Complete the upload sequence
        val = 0x01;
        if (HAL_I2C_Mem_Write(&hi2c2, s_bmi_addr, BMI270_REG_INIT_CTRL, 1, &val, 1, 100)) {
            return IMU_ERROR;
        }
        HAL_Delay(20); // wait 20ms for the internal ASIC to boot and verify CRC
    }

    // verify that the ASIC successfully initialized
    uint8_t init_status = 0;
    if (HAL_I2C_Mem_Read(&hi2c2, s_bmi_addr, 
        BMI270_REG_INTERNAL_STATUS, 1, &init_status, 1, 100) != HAL_OK || (init_status & 0x01) !=  0x01) {
        return IMU_ERROR; // bit 0 != 1 means the firmware failed to load
    }

    /* 
    Configure the Acceleromter
    ACC_CONF (0x40) = 0xA8 -> 0b10101000:
        Bit 7 = 1: High Performance mode
        Bits 6-4 = 010: Normal bandwidth filter
        Bits 3-0 = 1000: Output Data Rate = 100 Hz
    ACC_RANGE (0x41) = 0x00 -> +/- 2g range (16384 LSB/g)
    */
    val = 0xA8;
    HAL_I2C_Mem_Write(&hi2c2, s_bmi_addr, BMI270_REG_ACC_CONF, 1, &val, 1, 100);
    val = 0x00;
    HAL_I2C_Mem_Write(&hi2c2, s_bmi_addr, BMI270_REG_ACC_RANGE, 1, &val, 1, 100);

    /*
    Configure Gyroscope:
    GYR_CONF (0x42) = 0xE8 -> 0b11101000:
        Bit 7 = 1: High performance filter
        Bit 6 = 1: High performance noise mode
        Bits 5-4 = 10: Normal filter
        Bit 3-0 = 1000: Output Data Rate = 100 Hz
    BYR_RANGE (0x43) = 0x00 -> +/- 2000 deg/s range (16.4 LSB/dps)
    */
    val = 0xE8;
    HAL_I2C_Mem_Write(&hi2c2, s_bmi_addr, BMI270_REG_GYR_CONF, 1, &val, 1, 100);
    val = 0x00;
    HAL_I2C_Mem_Write(&hi2c2, s_bmi_addr, BMI270_REG_GYR_RANGE, 1, &val, 1, 100);

    /*
    Power On Sensors:
    PWR_CTRL (0x7D) - 0x0E -> 0b00001110:
        Bit 3 = 1: Temperature Sensor enable
        Bit 2 = 1: Acceleromter enable
        Bit 1 = 1: Gyroscope enable
    */
    val = 0x0E;
    if (HAL_I2C_Mem_Write(&hi2c2, s_bmi_addr, BMI270_REG_PWR_CTRL, 1, &val, 1, 100) != HAL_OK) {
        return IMU_ERROR;
    }

    return IMU_OK;
}

imu_status_t imu_read(imu_sample_t *sample) {
    if (sample == NULL) return IMU_ERROR;

    uint8_t raw[14];

    /* Burst read: Accel (6B) + Temp (2B) + Gyro (6B) = 14 bytes */
    if (HAL_I2C_Mem_Read(&hi2c2, s_mpu_addr, MPU6050_REG_DATA_START, 1, raw, 14, 50) != HAL_OK) {
        return IMU_ERROR;
    }

    sample->timestamp_us = adcs_monotonic_time_us();

    int16_t ax = (int16_t)((raw[0] << 8) | raw[1]);
    int16_t ay = (int16_t)((raw[2] << 8) | raw[3]);
    int16_t az = (int16_t)((raw[4] << 8) | raw[5]);

    int16_t temp_raw = (int16_t)((raw[6] << 8) | raw[7]);

    int16_t gx = (int16_t)((raw[8] << 8) | raw[9]);
    int16_t gy = (int16_t)((raw[10] << 8) | raw[11]);
    int16_t gz = (int16_t)((raw[12] << 8) | raw[13]);

    /* Accel scale: +/- 2g range -> 16384 LSB/g, convert to m/s^2 */
    const float accel_scale = 9.80665F / 16384.0F;
    sample->acceleration_m_s2[0] = (float)ax * accel_scale;
    sample->acceleration_m_s2[1] = (float)ay * accel_scale;
    sample->acceleration_m_s2[2] = (float)az * accel_scale;

    /* Temperature: formula from InvenSense MPU-6050 register map */
    sample->temperature_c = ((float)temp_raw / 340.0F) + 36.53F;

    /* Gyro scale: +/- 2000 dps range -> 16.4 LSB/(deg/s), convert to rad/s */
    const float gyro_scale = (2000.0F / 32768.0F) * (3.1415926535F / 180.0F);
    sample->angular_rate_rad_s[0] = (float)gx * gyro_scale;
    sample->angular_rate_rad_s[1] = (float)gy * gyro_scale;
    sample->angular_rate_rad_s[2] = (float)gz * gyro_scale;

    return IMU_OK;
}
