#ifndef THERMAL_DATA_H
#define THERMAL_DATA_H

#include <stdint.h>

#include "sensor_interface.h"

#define THERMALS_MIN_TARGET_TEMP_C (-40.0f) //please don't confuse F with Farenheit
#define THERMALS_MAX_TARGET_TEMP_C (125.0f)
//current placeholder limits for simulation
//for hw implementation we shall discuss and set appropriate limits

typedef struct {
    float temperatures[MAX_SENSORS];
    uint32_t valid_sensor_mask;
    /*
    * One validity bit per sensor:
    * bit 0 = temperatures[0] contains a valid sensor 1 reading
    * bit 1 = temperatures[1] contains a valid sensor 2 reading
    *
    * A set bit means that sensor has produced a valid reading.
    * A cleared bit means its temperature must not be used.
    */
    float average_temp;
    float target_temp;
    uint8_t target_temp_valid;

} ThermalData_t;

void thermals_set_current(float temp, unsigned int sensor_id);

int thermals_set_target(float target);
/* 
* returns:
* 1 = accepted target
* 0 = rejected target
*/

int thermals_target_is_valid(float target);

void thermals_invalidate_sensor(unsigned int sensor_id);

ThermalData_t get_thermal_data(void);

#endif // THERMAL_DATA_H
