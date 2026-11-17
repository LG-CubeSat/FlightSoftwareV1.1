#ifndef EPS_DATA
#define EPS_DATA

#include <stdint.h>
#include <stdbool.h>

typedef enum {


    //Sensor masks
    EPS_SOLAR_VOLATGE               =1u << 0,
    EPS_SOLAR_CURRENT               =1u << 1,
    EPS_BATTERY_VOLTAGE             =1u << 2,
    EPS_BATTERY_CURRENT             =1u << 3,
    EPS_BATTERY_TEMPERATURE         =1u << 4,

    //Estimation masks
    SOLAR_POWER_W                   =1u << 5,
    BATTERY_POWER_W                 =1u << 6,
    ESTIMATED_POWER_BALANCE         =1u << 7,
    BATTERY_ENERGY_IN_WH            =1u << 8,
    BATTERY_ENERGY_OUT_WH           =1u << 9,
    ESTIMATED_STATE_OF_CHARGE_PCT   =1U << 10,
    

    //Smoothed solar-power average; used to predict charging availability
    float average_solar_power_w;
    //Smoothed battery-current average; used to predict battery trend
    float average_battery_current_a;
    //Predicted time until empty; used to trigger power-saving mode
    float estimated_time_to_empty_h;
    //Predicted time until full; used to track charging progress
    float estimated_time_to_full_h;
    

} eps_valid_mask_t;

typedef struct {

    //other data

    //Time of the sample; used to calculate elapsed time
    uint32_t timestamp_ms;
    //New-sample counter; used to avoid processing a sample twice
    uint32_t sequence;
    //Binary mask; used to detemine which data is valid
    uint32_t valid_sensor_mask;


    //solar readings

    //Solar input voltage; used to calculate solar power
    float solar_voltage_reading;
    //Solar input current; used to calculate solar power
    float solar_current_reading;
    //Solar shunt voltage drop; used to verify solar current
    float solar_shunt_voltage_reading;
    //Sensor-reported solar power; used to check calculated solar power
    float solar_power_reading;

    //battery readings

    //Battery voltage; used for power and safety checks
    float battery_voltage_reading;
    //Battery charge/discharge current; used for power and energy calculations
    float battery_current_reading;
    //Battery shunt voltage drop; used to verify battery current.
    float battery_shunt_voltage_reading;
    //Sensor-reported battery power; used to check calculated battery power
    float battery_power_reading;
    //temperature reading from battery; used to check for overheating
    float battery_temperature_reading;


} eps_sensor_data_t;

typedef struct {

    //Binary mask; used to detemine which data is valid
    uint32_t valid_estimation_mask;
    //Calculated solar power; used to measure solar generation
    float solar_power_w;
    //Calculated battery power; used to determine charging or discharging
    float battery_power_w;
    //Approximate spacecraft power use; used to detect high loads
    float estimated_power_balance_w;

    //Total energy charged into the battery; used for state of charge (SOC) estimation
    float battery_energy_in_wh;
    //Total energy removed from the battery; used for state of charge (SOC) estimation
    float battery_energy_out_wh;
    //Estimated battery fullness; used for low-power decisions (measured in %)
    float estimated_state_of_charge_pct;

    //Smoothed solar-power average; used to predict charging availability
    float average_solar_power_w;
    //Smoothed battery-current average; used to predict battery trend
    float average_battery_current_a;
    //Predicted time until empty; used to trigger power-saving mode
    float estimated_time_to_empty_h;
    //Predicted time until full; used to track charging progress
    float estimated_time_to_full_h;

} eps_estimation_data_t;

eps_sensor_data_t get_eps_sensor_data(void);

eps_estimation_data_t get_eps_estimation_data(void);


void set_eps_sensor_data(eps_sensor_data_t newdata);

void set_eps_estimation_data(eps_estimation_data_t newdata);



#endif //EPS_DATA