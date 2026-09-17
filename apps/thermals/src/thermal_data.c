#include "thermal_data.h"

static ThermalData_t thermal_data = {

    .current_temp = 0.00f,
    .target_temp = 0.00f,

};

// sets the current temperature

void thermals_set_current(float temp) {

    thermal_data.current_temp = temp;

}
// sets the goal temperature

void thermals_set_target(float goal) {

    thermal_data.target_temp = goal;

}
// returns thermals data

ThermalData_t get_thermal_data(void) {

    return thermal_data;
}