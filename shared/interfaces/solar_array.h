/* Hardware-agnostic contract for the EPS solar array. */
#ifndef SHARED_INTERFACES_SOLAR_ARRAY_H
#define SHARED_INTERFACES_SOLAR_ARRAY_H

#include <stdint.h>

typedef enum {
    SOLAR_ARRAY_OK = 0,
    SOLAR_ARRAY_ERROR = -1,
    SOLAR_ARRAY_NOT_READY = -2
} solar_array_status_t;

typedef struct {
    uint64_t timestamp_us;
    float voltage_v;
    float current_a;
    float irradiance_w_m2;
    uint8_t illuminated;   /* 0 = eclipse / no usable input */
} solar_array_sample_t;

solar_array_status_t solar_array_initialize(void);
solar_array_status_t solar_array_read(solar_array_sample_t *sample);

#endif
