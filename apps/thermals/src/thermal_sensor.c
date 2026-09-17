//This file mainly just turns on and makes sure all thermal sensor(s) 
//are working properly before letting other code features use them

#include "thermal_sensor.h"

#include <stddef.h>
#include "stdint.h"

static uint8_t sensor_count = 0;

int thermal_sensor_init(thermal_sensor_t *sensor, uint8_t address) {

    if (sensor == NULL)
    {
        return -1;
    }

    if (address > 0x7F) {
        return -1; // invalid address
    }
    if (sensor_count >= MAX_SENSORS) {
        return -2; // two sensors (MAX) already initialized
    }
    else sensor->address = address;
    sensor_count++;
    return 1; // successful initialization

}
/*
 * Reads the specified thermal sensor.
 *
 * On success:
 *   - stores the temperature in degrees Celsius at temperature_out
 *   - returns 1
 *
 * On failure:
 *   - does not provide a valid temperature
 *   - returns a negative error code
 */
int thermal_sensor_read(
    const thermal_sensor_t *sensor,
    float *temperature_out) {

    if (sensor == NULL || temperature_out == NULL) {
        return -1;
    }
    if (sensor -> sensor_id == 1) {
        // call read task for sensor 1
        *temperature_out = 0.00f; //0 is placeholder for data from sensor 1
        return 1; //succsessful read
    }
    else if (sensor -> sensor_id == 2) {
        // call read task for sensor 2
        *temperature_out = 0.00f; //0 is placeholder for data from sensor 2
        return 1; //succsessful read
    }
    else {
        return -1; // error occured
    }


}
