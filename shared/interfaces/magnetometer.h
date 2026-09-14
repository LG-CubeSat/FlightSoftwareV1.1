/* Hardware-agnostic contract for the ADCS three-axis magnetometer. */
#ifndef SHARED_INTERFACES_MAGNETOMETER_H
#define SHARED_INTERFACES_MAGNETOMETER_H

#include <stdint.h>

typedef enum {
    MAGNETOMETER_OK = 0,
    MAGNETOMETER_ERROR = -1,
    MAGNETOMETER_NOT_READY = -2
} magnetometer_status_t;

typedef struct {
    uint64_t timestamp_us;
    float magnetic_field_t[3];
} magnetometer_sample_t;

magnetometer_status_t magnetometer_initialize(void);
magnetometer_status_t magnetometer_read(magnetometer_sample_t *sample);

#endif
