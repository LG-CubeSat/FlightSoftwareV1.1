#include "tasks/housekeeping_task.h"

#include <stddef.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "manager/eps_manager.h"
#include "manager/fault_manager.h"
#include "simulation/eps_simulator.h"
#include "tasks/command_task.h"
#include "tasks/estimation_task.h"
#include "tasks/sensor_task.h"
#include "tasks/telemetry_task.h"

#define HOUSEKEEPING_TASK_PRIORITY 1
#define HOUSEKEEPING_TASK_STACK_SIZE 1536
#define HOUSEKEEPING_TASK_PERIOD_MS 1000
#define ALL_RECOVERABLE_FAULTS EPS_FAULT_ALL

static StackType_t housekeeping_task_stack[HOUSEKEEPING_TASK_STACK_SIZE];
static StaticTask_t housekeeping_task_buffer;

TaskHandle_t xHousekeepingHandle;

void housekeeping_task_init(void) {
    xHousekeepingHandle = xTaskCreateStatic(
        housekeeping_task,
        "Housekeeping",
        HOUSEKEEPING_TASK_STACK_SIZE,
        NULL,
        HOUSEKEEPING_TASK_PRIORITY,
        housekeeping_task_stack,
        &housekeeping_task_buffer);
    if (xHousekeepingHandle == NULL) {
        printf("[HEALTH] Task creation failed.\n");
    }
}

static float minimum_stack_margin(void) {
    TaskHandle_t handles[] = {
        xSensorHandle,
        xEstimationHandle,
        xCommandHandle,
        xTelemetryHandle,
        xHousekeepingHandle
    };
    UBaseType_t minimum = (UBaseType_t)-1;

    for (size_t index = 0U; index < sizeof(handles) / sizeof(handles[0]); ++index) {
        if (handles[index] != NULL) {
            UBaseType_t margin = uxTaskGetStackHighWaterMark(handles[index]);
            if (margin < minimum) {
                minimum = margin;
            }
        }
    }
    return minimum == (UBaseType_t)-1 ? 0.0F : (float)minimum;
}

void housekeeping_task(void *parameters) {
    TickType_t last_wake_time = xTaskGetTickCount();

    (void)parameters;
    for (;;) {
        eps_manager_state_t snapshot;
        eps_health_t health;
        uint32_t observed_faults;
        uint32_t latched_faults;
        uint64_t now_us;

        eps_manager_get_state(&snapshot);
        now_us = eps_simulator_get_time_us();
        observed_faults =
            fault_management_evaluate_eps(&snapshot.latest_sensors, now_us);
        latched_faults = fault_management_get_active();
        fault_management_clear(
            latched_faults & ALL_RECOVERABLE_FAULTS & ~observed_faults);
        if (observed_faults != EPS_FAULT_NONE) {
            fault_management_report((eps_fault_t)observed_faults);
        }

        health = snapshot.health;
        health.timestamp_us = now_us;
        health.active_faults = fault_management_get_active();
        health.sensors_healthy =
            (health.active_faults & EPS_FAULT_SENSOR_STALE) == 0U;
        health.minimum_stack_margin_words = minimum_stack_margin();
        eps_manager_set_health(&health);
        eps_manager_update(now_us);
        fault_management_pet();
        xTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(HOUSEKEEPING_TASK_PERIOD_MS));
    }
}
