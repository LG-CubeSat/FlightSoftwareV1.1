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

    //estiamtion data here


} eps_estimation_data_t;


#endif //EPS_DATA