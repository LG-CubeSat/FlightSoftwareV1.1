#include "imu.h"
#include "main.h"
#include "platform/adcs_environment.h"
#include <stdio.h>

extern I2C_HandleTypeDef hi2c2;

#define MPU6050_I2C_ADDR_LOW   (0x68 << 1) /* AD0 pulled low (GND) */
#define MPU6050_I2C_ADDR_HIGH  (0x69 << 1) /* AD0 pulled high (3.3V) */

#define MPU6050_REG_SMPLRT_DIV 0x19
#define MPU6050_REG_CONFIG     0x1A
#define MPU6050_REG_GYRO_CFG   0x1B
#define MPU6050_REG_ACCEL_CFG  0x1C
#define MPU6050_REG_DATA_START 0x3B
#define MPU6050_REG_PWR_MGMT_1 0x6B
#define MPU6050_REG_WHO_AM_I   0x75

static uint16_t s_mpu_addr = MPU6050_I2C_ADDR_LOW;

imu_status_t imu_initialize(void) {
    uint8_t who_am_i = 0;

    /* Probe default I2C address (0x68) */
    s_mpu_addr = MPU6050_I2C_ADDR_LOW;
    if (HAL_I2C_Mem_Read(&hi2c2, s_mpu_addr, MPU6050_REG_WHO_AM_I, 1, &who_am_i, 1, 100) != HAL_OK || who_am_i != 0x68) {
        /* Probe alternate address (0x69) if AD0 is tied high */
        s_mpu_addr = MPU6050_I2C_ADDR_HIGH;
        if (HAL_I2C_Mem_Read(&hi2c2, s_mpu_addr, MPU6050_REG_WHO_AM_I, 1, &who_am_i, 1, 100) != HAL_OK || who_am_i != 0x68) {
            return IMU_NOT_READY;
        }
    }

    /* Wake up MPU6050 and set clock source to PLL with X-axis gyro reference (0x01) */
    uint8_t pwr = 0x01;
    if (HAL_I2C_Mem_Write(&hi2c2, s_mpu_addr, MPU6050_REG_PWR_MGMT_1, 1, &pwr, 1, 100) != HAL_OK) {
        return IMU_ERROR;
    }
    HAL_Delay(10);

    /* Configure DLPF: ~42 Hz bandwidth to filter bench vibration (DLPF_CFG = 3) */
    uint8_t dlpf_cfg = 0x03;
    HAL_I2C_Mem_Write(&hi2c2, s_mpu_addr, MPU6050_REG_CONFIG, 1, &dlpf_cfg, 1, 100);

    /* Sample rate divider: 1 kHz / (1 + 4) = 200 Hz */
    uint8_t smplrt = 0x04;
    HAL_I2C_Mem_Write(&hi2c2, s_mpu_addr, MPU6050_REG_SMPLRT_DIV, 1, &smplrt, 1, 100);

    /* Gyro full scale: +/- 2000 deg/s (FS_SEL = 3 -> 0x18) */
    uint8_t gyro_cfg = 0x18;
    HAL_I2C_Mem_Write(&hi2c2, s_mpu_addr, MPU6050_REG_GYRO_CFG, 1, &gyro_cfg, 1, 100);

    /* Accel full scale: +/- 2g (AFS_SEL = 0 -> 0x00) */
    uint8_t accel_cfg = 0x00;
    HAL_I2C_Mem_Write(&hi2c2, s_mpu_addr, MPU6050_REG_ACCEL_CFG, 1, &accel_cfg, 1, 100);

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
