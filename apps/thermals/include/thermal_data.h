#ifndef THERMAL_DATA_H
#define THERMAL_DATA_H

#include <stdint.h>

#define MAX_SENSORS (2)

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

} ThermalData_t;

void thermals_set_current(float temp, unsigned int sensor_id);

void thermals_set_target(float goal);

void thermals_invalidate_sensor(unsigned int sensor_id);

ThermalData_t get_thermal_data(void);

#endif //THERMAL_DATA_H