#include <pthread.h>

#include "eps_data.h"
#include <math.h>


/*
split up the locks into different sections in order to not block one thing while something else is running as there are a lot
of different sensors and whatnot. I didn't include a lock for every single data point though because it would be too chaotic;
chose a nice in between point. 
*/
static pthread_mutex_t solar_sensor_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t battery_sensor_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t battery_estimation_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t solar_estimation_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t other_data_lock = PTHREAD_MUTEX_INITIALIZER;

static eps_sensor_data_t eps_sensor_data = {

    .timestamp_ms = 0u, //to be implemeneted
    .sequence = 0u, //to be implemented
    .valid_mask = 0u << 0, //initial mask

    //solar readings
    .solar_voltage_reading = 0.00f,
    .solar_current_reading = 0.00f,
    .solar_shunt_voltage_reading = 0.00f,
    .solar_power_reading = 0.00f,

    //battery readings
    .battery_voltage_reading = 0.00f,
    .battery_current_reading = 0.00f, 
    .battery_shunt_voltage_reading = 0.00f,
    .battery_power_reading = 0.00f
};

static eps_estimation_data_t eps_estimation_data = {

    .solar_power_w = 0.00f,
    .battery_power_w =0.00f,
    .estimated_power_balance_w = 0.00f,

    .battery_energy_in_wh = 0.00f,
    .battery_energy_out_wh = 0.00f,
    .estimated_state_of_charge_pct = 0.00f,

    .average_solar_power_w = 0.00f,
    .average_battery_current_a = 0.00f,
    .estimated_time_to_empty_h = 0.00f,
    .estimated_time_to_full_h = 0.00f
    
};

eps_sensor_data_t get_eps_sensor_data(void) {

    eps_sensor_data_t snapshot;
    pthread_mutex_lock(&)


}