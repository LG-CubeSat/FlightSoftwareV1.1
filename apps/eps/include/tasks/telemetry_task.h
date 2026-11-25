/*
Telemetry Task
1-10HZ
Collects everything
Sends out data using CSP
*/

#ifndef TELEMETRY_TASK_H
#define TELEMETRY_TASK_H

/* EPS -> OBC telemetry: current simulator payload. */
typedef struct {
    // Sensor snapshot identification.
    uint32_t sensor_timestamp_ms;
    uint32_t sensor_sequence;

    // Validity.
    uint32_t valid_sensor_mask;
    uint32_t valid_estimation_mask;

    // Sensor readings.
    float solar_voltage_v;
    float solar_current_a;
    float solar_shunt_voltage_v;

    float battery_voltage_v;
    float battery_current_a;
    float battery_shunt_voltage_v;
    float battery_temperature_c;

    // Estimation data.
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

    // 1=charging, -1=discharging, 0=idle, 2=invalid.
    int8_t state_of_flow;
} eps_telemetry_t;

#include "FreeRTOS.h"
#include "task.h"

extern TaskHandle_t xTelemetryHandle;

void telemetry_task_init(void);

void telemetry_task(void *pvParamaters);

#endif