/*
Telemetry Task
1-10HZ
Collects everything
Sends out data using CSP
*/

#include "tasks/telemetry_task.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>
#include <string.h>

#include <stdbool.h>
#include <stdatomic.h>

#include "eps_data.h"

#include <csp/csp.h>

#include "csp_commands.h"

#define TELEMETRY_TASK_PRIORITY (1)
#define TELEMETRY_TASK_STACK_SIZE (1024)
#define TELEMETRY_TASK_PERIOD_MS (1000)
#define TELEMETRY_REQUEST_POLL_MS (20)
#define TELEMETRY_RETRY_MS (200)

static atomic_bool telemetry_send_requested = false;
static atomic_bool telemetry_ready = false;

_Static_assert(sizeof(eps_telemetry_t) <= CSP_BUFFER_SIZE,
               "EPS telemetry exceeds CSP packet capacity");

static StackType_t xTelemetryTaskStack[TELEMETRY_TASK_STACK_SIZE];
static StaticTask_t xTelemetryTaskBuffer;

TaskHandle_t xTelemetryHandle = NULL;

bool telemetry_request_send(void)
{
    if (!atomic_load(&telemetry_ready)) {
        return false;
    }
    atomic_store(&telemetry_send_requested, true);
    return true;
}

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
        printf("[TELEMETRY] Failed to initialize.\n");
    }
    else
    {
        atomic_store(&telemetry_ready, true);
        printf("[TELEMETRY] Successfully initialized\n");
        fflush(stdout);
    }
}

void telemetry_task(void *pvParameters)
{
    (void) pvParameters;

    const TickType_t report_period =
        pdMS_TO_TICKS(TELEMETRY_TASK_PERIOD_MS);

    const TickType_t poll_period =
        pdMS_TO_TICKS(TELEMETRY_REQUEST_POLL_MS);

    TickType_t last_periodic_tick =
        xTaskGetTickCount() - report_period;
    TickType_t last_attempt_tick = 0;
    bool retry_pending = false;

    for (;;)
    {
        TickType_t now = xTaskGetTickCount();

        if (retry_pending &&
            (TickType_t)(now - last_attempt_tick) <
                pdMS_TO_TICKS(TELEMETRY_RETRY_MS)) {
            vTaskDelay(poll_period);
            continue;
        }

        bool requested =
            atomic_exchange(&telemetry_send_requested, false);

        bool periodic_due =
            (TickType_t)(now - last_periodic_tick) >= report_period;

        if (!requested && !periodic_due && !retry_pending) {
            vTaskDelay(poll_period);
            continue;
        }

        if (periodic_due) {
            last_periodic_tick = now;
        }
        last_attempt_tick = now;
        bool handed_to_csp = false;
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

        // Open a connection to the OBC's EPS telemetry port.
        csp_conn_t *connection = csp_connect(
            CSP_PRIO_NORM,
            OBC_ADDRESS,
            EPS_TELEM_PORT,
            100,
            CSP_O_NONE
        );

        if (connection != NULL) {
            // Request a packet from CSP's buffer pool.
            csp_packet_t *packet = csp_buffer_get(0);

            if (packet != NULL) {
                // Copy the completed report into the packet.
                memcpy(packet->data, &telemetry, sizeof(telemetry));
                packet->length = (uint16_t)sizeof(telemetry);

                // Send it. CSP takes ownership of the packet.
                csp_send(connection, packet);
                handed_to_csp = true;
            }

            // Close the connection even if packet allocation failed.
            csp_close(connection);
        }

        if (!handed_to_csp && !retry_pending) {
            fprintf(stderr, "[TELEMETRY] Connection/buffer unavailable; retrying.\n");
        }
        retry_pending = !handed_to_csp;
        vTaskDelay(poll_period);
    }
}
