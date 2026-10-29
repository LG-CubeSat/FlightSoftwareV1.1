#include "imu.h"
#include "main.h"
#include "platform/adcs_environment.h"
#include <stdio.h>

extern I2C_HandleTypeDef hi2c2;

#define MPU6050_I2C_ADDR (0x68 << 1) /* Replace with the real IMU 7 bit address << 1 */
#define MPU6050_REG_PWR_MGMT_1 0x6B
#define MPU6050_REG_GYRO_CFG 0x1B
#define MPU6050_REG_ACCEL_CFG 0x1C
#define MPU6050_REG_DATA_START 0x3B
#define MPU6050_REG_WHO_AM_I 0x75

imu_status_t imu_initialize(void) {
    // check device id and wake sensor from sleep
    uint8_t who_am_i = 0;
    if (HAL_I2C_Mem_Read(
        &hi2c2, MPU6050_I2C_ADDR, MPU6050_REG_WHO_AM_I, 1, &who_am_i, 1, 100
    ) != HAL_OK) {
        return IMU_NOT_READY;
    }
    if (who_am_i != 0x68) {
        return IMU_NOT_READY;
    }

    // wake up MPU6050 by writing 0x00 to PWR_MGMT_1. This takes it out of sleep
    uint8_t pwr = 0x00;
    if (HAL_I2C_Mem_Write(&hi2c2, MPU6050_I2C_ADDR, MPU6050_REG_PWR_MGMT_1, 1, &pwr, 1, 100) != HAL_OK) {
        return IMU_ERROR;
    }

    // configure gyro full scale: +/- 2000 deg/s (FS_SEL = 3 -> 0x18)
    uint8_t gyro_cfg = 0x18;
    HAL_I2C_Mem_Write(&hi2c2, MPU6050_I2C_ADDR, MPU6050_REG_GYRO_CFG, 1, &gyro_cfg, 1, 100);

    // configure accell full scale: +/- 2g (AFS_SEL = 0 -> 0x00)
    uint8_t accel_cfg = 0x00;
    HAL_I2C_Mem_Write(&hi2c2, MPU6050_I2C_ADDR, MPU6050_REG_ACCEL_CFG, 1, &accel_cfg, 1, 100);

    // configure ODR and power registrations here
    return IMU_OK;
}

imu_status_t imu_read(imu_sample_t *sample) {
    if (sample == NULL) return IMU_ERROR;

    uint8_t raw[14];

    /* Burst read accel (6b) + temp (2B) + gyro (6B) */
    if (HAL_I2C_Mem_Read(&hi2c2, IMU_I2C_ADDR, 0x3B, 1, raw, 14, 50) != HAL_OK) {
        return IMU_ERROR;
    }

    sample->timestamp_us = adcs_monotonic_time_us();

    // convert raw 16 bit counts to rad/s
    int16_t gx = (int16_t)(raw[8] << 8 | raw[9]);
    int16_t gy = (int16_t)(raw[10] << 8 | raw[11]);
    int16_t gz = (int16_t)((raw[12] << 8) | raw[13]);

    const float rad_per_lsb = (2000.0f / 32768.0f) * (3.14159265f / 180.0f);
    sample->angular_rate_rad_s[0] = (float)gx * rad_per_lsb;
    sample->angular_rate_rad_s[1] = (float)gy * rad_per_lsb;
    sample->angular_rate_rad_s[2] = (float)gz * rad_per_lsb;

    return IMU_OK;
}