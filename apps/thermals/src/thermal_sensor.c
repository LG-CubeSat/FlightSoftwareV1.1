
#include "thermal_sensor.h"
#include "stdint.h"

static uint8_t sensor_count = 0;

int thermal_sensor_init(thermal_sensor_t *sensor, uint8_t address) {

    if (address > 0x7F) {
        return -1; // invalid address
    }
    if (sensor_count >= 2) {
        return -2; // two sensors (MAX) already initialized
    }
    else sensor->ADDRESS = address;
    sensor_count++;
    return 1; // successful initialization

}

float thermal_sensor_read(thermal_sensor_t *sensor) {

    if (sensor -> SENSOR_GENERIC == 1) {
        // call read task for sensor 1
        return 0;
    }
    else if (sensor -> SENSOR_GENERIC == 2) {
        // call read task for sensor 2
        return 0;
    }
    else {
        return -1.0; // invalid sensor
    }


}
