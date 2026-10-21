#ifndef SENSOR_INTERFACE_H
#define SENSOR_INTERFACE_H

#include <stdint.h>

#define MAX_SENSORS (2)

typedef struct
{
    uint8_t address;   // 7-bit I2C sensor address
    uint8_t sensor_id; // One-based logical sensor identifier
} thermal_sensor_t;


/*
Initializes a sensor using its configured address.
Returns a positive value on success or a negative error code on failure.
*/
int thermal_sensor_init(thermal_sensor_t *sensor, uint8_t address);

/*
Reads a sensor and stores the Celsius result in temperature_out.
Returns a positive value on success or a negative error code on failure.
*/
int thermal_sensor_read(const thermal_sensor_t *sensor, float *temperature_out);

#endif // SENSOR_INTERFACE_H, THERMAL_SENSOR_H
