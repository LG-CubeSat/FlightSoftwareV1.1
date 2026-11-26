#ifndef EPS_DATA
#define EPS_DATA

#include <stdint.h>
#include <pthread.h>
#include <math.h>

extern pthread_mutex_t eps_data_lock;
extern pthread_mutex_t eps_fault_data_lock;

typedef enum {


    //Sensor masks
    EPS_SOLAR_VOLTAGE               =1u << 0,
    EPS_SOLAR_CURRENT               =1u << 1,
    EPS_BATTERY_VOLTAGE             =1u << 2,
    EPS_BATTERY_CURRENT             =1u << 3,
    EPS_BATTERY_TEMPERATURE         =1u << 4,
    SOLAR_SHUNT_VOLTAGE             =1u << 5,
    BATTERY_SHUNT_VOLTAGE           =1u << 6,

    //Estimation masks
    SOLAR_POWER_W                   =1u << 7,
    BATTERY_POWER_W                 =1u << 8,
    ESTIMATED_POWER_BALANCE         =1u << 9,
    BATTERY_ENERGY_IN_WH            =1u << 10,
    BATTERY_ENERGY_OUT_WH           =1u << 11,
    ESTIMATED_STATE_OF_CHARGE_PCT   =1u << 12,
    AVERAGE_SOLAR_POWER_W           =1u << 13,
    AVERAGE_BATTERY_CURRENT_A       =1u << 14,
    ESTIMATED_TIME_TO_FULL_H        =1u << 15,
    ESTIAMTED_TIME_TO_EMPTY_H       =1u << 16,
    STATE_OF_FLOW                   =1u << 17,
    UPTIME_MIN                      =1u << 18,
    

} eps_valid_mask_t;

//NOTE SENSOR AND ESTIMATION MASKS DO NOT MIX WITH FAULT MASKS

typedef enum {

    BATT_HIGH_VOLTAGE               =1u << 1,
    BATT_HIGH_CURRENT               =1u << 2,
    BATT_LOW_VOLTAGE                =1u << 3,
    BATT_LOW_CURRNET                =1u << 4,
    BATT_HIGH_TEMP                  =1u << 5,
    BATT_LOW_TEMP                   =1u << 6
    //for warnings and critical masks

    
} fault_masks_t;

typedef struct {

    //other data

    //Time of the sample; used to calculate elapsed time
    uint32_t timestamp_ms;
    //New-sample counter; used to avoid processing a sample twice
    uint32_t sequence;
    //Binary mask; used to detemine which data is valid
    uint8_t valid_sensor_mask;


    //solar readings

    //Solar input voltage; used to calculate solar power
    float solar_voltage_reading;
    //Solar input current; used to calculate solar power
    float solar_current_reading;
    //Solar shunt voltage drop; used to verify solar current
    float solar_shunt_voltage_reading;


    //battery readings

    //Battery voltage; used for power and safety checks
    float battery_voltage_reading;
    //Battery charge/discharge current; used for power and energy calculations
    float battery_current_reading;
    //Battery shunt voltage drop; used to verify battery current.
    float battery_shunt_voltage_reading;
    //temperature reading from battery; used to check for overheating
    float battery_temperature_reading;
    //total valid sensor data points
    uint8_t valid_sensor_points;
    //total invalid estimation data points
    uint8_t invalid_sensor_points;
    //percentage of valid points
    float valid_sensor_points_pct;


} eps_sensor_data_t;

typedef struct {

    //Binary mask; used to detemine which data is valid
    uint32_t valid_estimation_mask;
    //State of energy, discharging or charging used for power in/out calculations
    int8_t state_of_flow; //1 for charging, -1 for discarhging, 0 for idle, 2 for NaN/Na
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
    //total eps uptime
    float uptime_min;
    //total valid estimation data points
    uint8_t valid_estimation_points;
    //total invalid estimation data points
    uint8_t invalid_estimation_points;
    //percentage of valid points
    float valid_estimation_points_pct;


} eps_estimation_data_t;

enum eps_state {

    EPS_STATE_ERR, //error determining state
    EPS_STATE_SAFE,
    EPS_STATE_WARNING,
    EPS_STATE_CRITICAL, 
    EPS_STATE_IDLE, 
    EPS_STATE_ECLIPSE, 
    EPS_STATE_NOMINAL,
    EPS_STATE_NA 

    /*
    PRIORITY LADDER

    CRITICAL STATE
    ECLIPSE STATE
    SAFE STATE
    WARNING STATE 
    NOMINAL STATE
    IDLE SATE
    N/A
    
    */

};

typedef struct {

    enum eps_state current_state;
    enum eps_state previous_state;


    uint32_t active_warnings;
    uint32_t active_faults; 

    uint32_t time_spent_in_current_state_ms;
    


} eps_fault_data_t;


eps_fault_data_t get_eps_fault_data(void);

eps_sensor_data_t get_eps_sensor_data(void);

eps_estimation_data_t get_eps_estimation_data(void);


void set_eps_sensor_data(eps_sensor_data_t newdata);

void set_eps_estimation_data(eps_estimation_data_t newdata);

void set_eps_fault_data(eps_fault_data_t newdata);


#endif //EPS_DATA