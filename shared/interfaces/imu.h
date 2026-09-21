/* Hardware-agnostic contract for the ADCS inertial measurement unit. */
#ifndef SHARED_INTERFACES_IMU_H
#define SHARED_INTERFACES_IMU_H

#include <stdint.h>

typedef enum {
    IMU_OK = 0,
    IMU_ERROR = -1,
    IMU_NOT_READY = -2
} imu_status_t;

typedef struct {
    uint64_t timestamp_us;
    float angular_rate_rad_s[3];
    float acceleration_m_s2[3];
    float temperature_c;
} imu_sample_t;

imu_status_t imu_initialize(void);
imu_status_t imu_read(imu_sample_t *sample);

#endif
