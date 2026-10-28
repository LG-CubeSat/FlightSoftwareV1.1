#include "imu.h"
#include "main.h"
#include "platform/adcs_environment.h"
#include <stdio.h>

extern I2C_HandleTypeDef hi2c2;

#define IMU_I2C_ADDR (0x68 << 1) /* Replace with the real IMU 7 bit address << 1 */

imu_status_t imu_initialize(void) {
    // check device id and wake sensor from sleep
    uint8_t who_am_i = 0;
    if (HAL_I2C_Mem_Read(
        &hi2c2, IMU_I2C_ADDR, 0x75, 1, &who_am_i, 1, 1000
    ) != HAL_OK) {
        return IMU_NOT_READY;
    }
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