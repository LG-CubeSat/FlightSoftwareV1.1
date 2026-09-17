
#include "thermal_data.h"
#include "FreeRTOS.h"
#include "task.h"


static ThermalData_t thermal_data = {

    .temperatures = {0.00f, 0.00f}, //may need to adjust according to the number of sensors
    .valid_sensor_mask = 0u,
        /*
    * One validity bit per sensor:
    * bit 0 = temperatures[0] contains a valid sensor 1 reading
    * bit 1 = temperatures[1] contains a valid sensor 2 reading
    *
    * A set bit means that sensor has produced a valid reading.
    * A cleared bit means its temperature must not be used.
    */
    .average_temp = 0.00f,
    .target_temp = 0.00f,

};


static void thermals_recalculate_average(void)
{
    float sum = 0.0f;
    unsigned int valid_count = 0;

    for (unsigned int i = 0; i < MAX_SENSORS; i++)
    {
        if ((thermal_data.valid_sensor_mask & (1u << i)) != 0u)
        {
            sum += thermal_data.temperatures[i];
            valid_count++;
        }
    }

    if (valid_count > 0)
    {
        thermal_data.average_temp =
            sum / (float)valid_count;
    }
    else
    {
        thermal_data.average_temp = 0.0f;
    }
}

// sets the current temperature(s)
void thermals_set_current(float temp, unsigned int sensor_id)
{
    if (sensor_id == 0 || sensor_id > MAX_SENSORS)
    {
        return;
    }

    thermal_data.temperatures[sensor_id - 1] = temp;
    thermal_data.valid_sensor_mask |= (1u << (sensor_id - 1));

    thermals_recalculate_average();
}

// sets the goal temperature

void thermals_set_target(float target) {

    thermal_data.target_temp = target;

}
// returns thermals data

ThermalData_t get_thermal_data(void) {

    return thermal_data;
}

void thermals_invalidate_sensor(unsigned int sensor_id)
{
    if (sensor_id == 0 || sensor_id > MAX_SENSORS)
    {
        return;
    }

    thermal_data.valid_sensor_mask &=
        ~(1u << (sensor_id - 1));

    thermals_recalculate_average();
}