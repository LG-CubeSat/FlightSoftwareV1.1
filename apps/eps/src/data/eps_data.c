#include <pthread.h>

#include "eps_data.h"
#include <math.h>

pthread_mutex_t eps_data_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t eps_fault_data_lock = PTHREAD_MUTEX_INITIALIZER;

//sensor_data definitions in eps_data.h
static eps_sensor_data_t eps_sensor_data = {

    .timestamp_ms = 0u, //to be implemeneted
    .sequence = 0u, //to be implemented
    .valid_sensor_mask = 0u << 0, //initial mask

    //solar readings
    .solar_voltage_reading = 0.00f,
    .solar_current_reading = 0.00f,
    .solar_shunt_voltage_reading = 0.00f,

    //battery readings
    .battery_voltage_reading = 0.00f,
    .battery_current_reading = 0.00f, 
    .battery_shunt_voltage_reading = 0.00f,
    .battery_temperature_reading = 0.00f
};

//estimation_data definitions in eps_data.h
static eps_estimation_data_t eps_estimation_data = {

    .valid_estimation_mask = 0u << 0,

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

static eps_fault_data_t eps_fault_data = {

    .current_state = EPS_STATE_NA,
    .previous_state = EPS_STATE_NA,

    .active_errors = 0U << 0U,

    .time_spent_in_current_state_ms = 0u

};

eps_fault_data_t get_eps_fault_data(void) {

    eps_fault_data_t snapshot;
    pthread_mutex_lock(&eps_fault_data_lock);
    snapshot = eps_fault_data;
    pthread_mutex_unlock(&eps_fault_data_lock);

    return snapshot;
}

eps_sensor_data_t get_eps_sensor_data(void) {

    eps_sensor_data_t snapshot;
    pthread_mutex_lock(&eps_data_lock);
    snapshot = eps_sensor_data;
    pthread_mutex_unlock(&eps_data_lock);

    return snapshot;

}

eps_estimation_data_t get_eps_estimation_data(void) {

    eps_estimation_data_t snapshot;
    pthread_mutex_lock(&eps_data_lock);
    snapshot = eps_estimation_data;
    pthread_mutex_unlock(&eps_data_lock);

    return snapshot;

}

void set_eps_sensor_data(eps_sensor_data_t newdata) {

    pthread_mutex_lock(&eps_data_lock);
    eps_sensor_data = newdata;
    pthread_mutex_unlock(&eps_data_lock);
    

}

void set_eps_estimation_data(eps_estimation_data_t newdata) {

    pthread_mutex_lock(&eps_data_lock);
    eps_estimation_data = newdata;
    pthread_mutex_unlock(&eps_data_lock);
    

}

void set_eps_fault_data(eps_fault_data_t newdata) {

    pthread_mutex_lock(&eps_fault_data_lock);
    eps_fault_data = newdata;
    pthread_mutex_unlock(&eps_fault_data_lock);

}