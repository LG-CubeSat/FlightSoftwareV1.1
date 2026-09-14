/* Hardware-agnostic contract for a coarse body-frame sun sensor. */
#ifndef SHARED_INTERFACES_SUN_SENSOR_H
#define SHARED_INTERFACES_SUN_SENSOR_H

#include <stdint.h>

typedef enum {
    SUN_SENSOR_OK = 0,
    SUN_SENSOR_ERROR = -1,
    SUN_SENSOR_NOT_READY = -2
} sun_sensor_status_t;

typedef struct {
    uint64_t timestamp_us;
    float sun_vector_body[3];
    float irradiance_w_m2;
    uint8_t visible;
} sun_sensor_sample_t;

sun_sensor_status_t sun_sensor_initialize(void);
sun_sensor_status_t sun_sensor_read(sun_sensor_sample_t *sample);

#endif
