/*
Telemetry Task
1-10HZ
Collects everything
Sends out data using CSP
*/

#include "../../include/tasks/telemetry_task.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>
#include <string.h>

#include "eps_data.h"

#include <csp/csp.h>

#include "csp_commands.h"

#define TELEMETRY_TASK_PRIORITY (1)
#define TELEMETRY_TASK_STACK_SIZE (1024)
#define TELEMETRY_TASK_PERIOD_MS (1000)

static StackType_t xTelemetryTaskStack[TELEMETRY_TASK_STACK_SIZE];
static StaticTask_t xTelemetryTaskBuffer;

TaskHandle_t xTelemetryHandle = NULL;

void telemetry_task_init(void)
{
    xTelemetryHandle = xTaskCreateStatic(
        telemetry_task,
        "telemetry",
        TELEMETRY_TASK_STACK_SIZE,
        NULL,
        TELEMETRY_TASK_PRIORITY,
        xTelemetryTaskStack,
        &xTelemetryTaskBuffer
    );

    if (xTelemetryHandle == NULL) {
        printf("[TELEMTRY] Failed to initialize.\n");
    }
    else
    {
        printf("[TELEMETRY] Task created successfully.\n");
    }
}

void telemetry_task(void *pvParameters)
{
    (void) pvParameters;

    TickType_t lastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        // Get private snapshots.
        eps_sensor_data_t sensor = get_eps_sensor_data();
        eps_estimation_data_t estimate = get_eps_estimation_data();

        // Build the report.
        eps_telemetry_t telemetry = {0};

        // Identify the sensor snapshot.
        telemetry.sensor_timestamp_ms = sensor.timestamp_ms;
        telemetry.sensor_sequence = sensor.sequence;

        // Copy validity information.
        telemetry.valid_sensor_mask = sensor.valid_sensor_mask;
        telemetry.valid_estimation_mask = estimate.valid_estimation_mask;

        // Copy sensor readings.
        telemetry.solar_voltage_v = sensor.solar_voltage_reading;
        telemetry.solar_current_a = sensor.solar_current_reading;
        telemetry.solar_shunt_voltage_v = sensor.solar_shunt_voltage_reading;

        telemetry.battery_voltage_v = sensor.battery_voltage_reading;
        telemetry.battery_current_a = sensor.battery_current_reading;
        telemetry.battery_shunt_voltage_v = sensor.battery_shunt_voltage_reading;
        telemetry.battery_temperature_c = sensor.battery_temperature_reading;

        // Copy estimates.
        telemetry.solar_power_w = estimate.solar_power_w;
        telemetry.battery_power_w = estimate.battery_power_w;
        telemetry.estimated_power_balance_w = estimate.estimated_power_balance_w;

        telemetry.battery_energy_in_wh = estimate.battery_energy_in_wh;
        telemetry.battery_energy_out_wh = estimate.battery_energy_out_wh;
        telemetry.estimated_state_of_charge_pct =
            estimate.estimated_state_of_charge_pct;

        telemetry.average_solar_power_w = estimate.average_solar_power_w;
        telemetry.average_battery_current_a = estimate.average_battery_current_a;

        telemetry.estimated_time_to_empty_h = estimate.estimated_time_to_empty_h;
        telemetry.estimated_time_to_full_h = estimate.estimated_time_to_full_h;

        telemetry.state_of_flow = estimate.state_of_flow;

        xTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(TELEMETRY_TASK_PERIOD_MS)
        );
    }
}
