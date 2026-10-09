#include "tasks/telemetry_task.h"

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "communication/telemetry.h"
#include "manager/eps_manager.h"

#define TELEMETRY_TASK_PRIORITY 1
#define TELEMETRY_TASK_STACK_SIZE 1792
#define TELEMETRY_TASK_PERIOD_MS 1000

static StackType_t telemetry_task_stack[TELEMETRY_TASK_STACK_SIZE];
static StaticTask_t telemetry_task_buffer;

TaskHandle_t xTelemetryHandle;

void telemetry_task_init(void) {
    eps_telemetry_init();
    xTelemetryHandle = xTaskCreateStatic(
        telemetry_task,
        "Telemetry",
        TELEMETRY_TASK_STACK_SIZE,
        NULL,
        TELEMETRY_TASK_PRIORITY,
        telemetry_task_stack,
        &telemetry_task_buffer);
    if (xTelemetryHandle == NULL) {
        printf("[TELEMETRY] Task creation failed.\n");
    }
}

void telemetry_task(void *parameters) {
    TickType_t last_wake_time = xTaskGetTickCount();
    uint32_t diagnostic_divider = 0U;

    (void)parameters;
    for (;;) {
        eps_manager_state_t snapshot;
        eps_telemetry_packet_t telemetry;
        uint32_t notification;

        eps_manager_get_state(&snapshot);
        memset(&telemetry, 0, sizeof(telemetry));
        telemetry.timestamp_us = snapshot.latest_sensors.timestamp_us;
        telemetry.mode = snapshot.mode;
        telemetry.sensors = snapshot.latest_sensors;
        telemetry.power = snapshot.latest_power;
        telemetry.health = snapshot.health;
        if (eps_telemetry_send(&telemetry) != EPS_TELEMETRY_OK) {
            eps_manager_note_dropped_message();
        }

        if (++diagnostic_divider >= 5U) {
            diagnostic_divider = 0U;
            printf("[EPS] mode=%s vbatt=%.2fV ibatt=%.2fA soc=%.2f "
                   "solar=%.2fV/%.2fA p=%.2fW faults=0x%02x\n",
                   eps_manager_mode_name(snapshot.mode),
                   snapshot.latest_sensors.pack_voltage_v,
                   snapshot.latest_sensors.pack_current_a,
                   snapshot.latest_sensors.state_of_charge,
                   snapshot.latest_sensors.solar_voltage_v,
                   snapshot.latest_sensors.solar_current_a,
                   snapshot.latest_power.instant_power_w,
                   snapshot.health.active_faults);
            fflush(stdout);
        }

        if (xTaskNotifyWait(0U, UINT32_MAX, &notification, 0U) == pdTRUE) {
            printf("[TELEMETRY] Reporting EPS state for request: %u\n",
                   notification);
            fflush(stdout);
        }
        xTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(TELEMETRY_TASK_PERIOD_MS));
    }
}
