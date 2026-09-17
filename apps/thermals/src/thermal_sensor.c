//This file mainly just turns on and makes sure all thermal sensor(s) 
//are working properly before letting other code features use them


#include "thermal_sensor.h"
#include "stdint.h"

#define MAX_SENSORS (2)

static uint8_t sensor_count = 0;

int thermal_sensor_init(thermal_sensor_t *sensor, uint8_t address) {

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

int thermal_sensor_read(
    const thermal_sensor_t *sensor,
    float *temperature_out) {

    if (sensor -> sensor_id == 1) {
        // call read task for sensor 1
        *temperature_out = 0.00f; //0 is placeholder for data from sensor 1
        return 1; //succsessful read
    }
    else if (sensor -> sensor_id == 2) {
        // call read task for sensor 2
        *temperature_out = 0; //0 is placeholder for data from sensor 2
        return 1; //succsessful read
    }
    else {
        return -1.0; // error occured
    }


}
