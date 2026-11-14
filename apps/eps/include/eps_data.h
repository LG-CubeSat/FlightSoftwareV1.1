#ifndef EPS_DATA
#define EPS_DATA

#include <stdint.h>

typedef enum {

    EPS_SOLAR_VOLATGE       =1u << 0,
    EPS_SOLAR_CURRENT       =1u << 1,
    EPS_BATTERY_VOLTAGE     =1u << 2,
    EPS_BATTERY_CURRENT     =1u << 3,
    EPS_BATTERY_TEMPERATURE =1u << 4

} eps_valid_mask_t;

typedef struct {

    uint32_t timestamp_ms;
    uint32_t sequence;
    uint32_t valid_mask;


    //solar readings
    float solar_voltage_reading;
    float solar_current_reading;
    float solar_shunt_voltage_reading;
    float solar_power_reading;

    //battery readings
    float battery_voltage_reading;
    float battery_current_reading;
    float battery_shunt_voltage_reading;
    float battery_power_reading;


} eps_sensor_data_t;

typedef struct {

    uint32_t source_sequence;
    uint32_t timestamp_ms;
    uint32_t valid_mask;
    uint32_t fault_mask;

    float solar_power_w;
    float battery_power_w;
    float estimated_power_balance_w;

    float battery_energy_in_wh;
    float battery_energy_out_wh;
    float estimated_state_of_charge_pct;

    float average_solar_power_w;
    float average_battery_current_a;
    float estimated_time_to_empty_h;
    float estimated_time_to_full_h;

} eps_estimation_data_t;

eps_sensor_data_t get_eps_sensor_data(void);

eps_estimation_data_t get_eps_estimation_data(void);

/*
because there are so many data points for both estimation and sensor data,
changing a data point will look something like this:
the function calling set_eps_sensor_data or set_eps_estimation_data will use the getter functions coorolated to its
data, then it will create it's own copy and tweak the data point it wishes to tweak,
then it will send that copy through the function and the data will be stored with the new copy

*/

void set_eps_sensor_data(eps_sensor_data_t newdata);

void set_eps_estimation_data(eps_estimation_data_t newdata);



#endif //EPS_DATA