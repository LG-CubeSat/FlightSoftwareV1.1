#include "tasks/housekeeping_task.h"

#include <float.h>
#include <stddef.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "manager/adcs_manager.h"
#include "manager/fault_manager.h"
#include "simulation/adcs_simulator.h"
#include "tasks/command_task.h"
#include "tasks/control_task.h"
#include "tasks/estimation_task.h"
#include "tasks/sensor_task.h"
#include "tasks/telemetry_task.h"

#define HOUSEKEEPING_TASK_PRIORITY 1
#define HOUSEKEEPING_TASK_STACK_SIZE 1280
#define HOUSEKEEPING_TASK_PERIOD_MS 1000
#define ALL_RECOVERABLE_FAULTS \
    (ADCS_FAULT_SENSOR_STALE | ADCS_FAULT_SENSOR_RANGE | \
     ADCS_FAULT_ATTITUDE_INVALID | ADCS_FAULT_EXCESSIVE_RATE | \
     ADCS_FAULT_ACTUATOR | ADCS_FAULT_TASK_DEADLINE)

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
        xControlHandle,
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
        adcs_manager_state_t snapshot;
        adcs_health_t health;
        uint32_t observed_faults;
        uint32_t latched_faults;
        uint64_t now_us;

        adcs_manager_get_state(&snapshot);
        now_us = adcs_simulator_get_time_us();
        observed_faults = fault_management_evaluate_adcs(
            &snapshot.latest_sensors,
            &snapshot.latest_attitude,
            &snapshot.latest_control,
            now_us);
        latched_faults = fault_management_get_active();
        fault_management_clear(
            latched_faults & ALL_RECOVERABLE_FAULTS & ~observed_faults);
        if (observed_faults != ADCS_FAULT_NONE) {
            fault_management_report((adcs_fault_t)observed_faults);
        }

        health = snapshot.health;
        health.timestamp_us = now_us;
        health.active_faults = fault_management_get_active();
        health.sensors_healthy =
            (health.active_faults &
             (ADCS_FAULT_SENSOR_STALE | ADCS_FAULT_SENSOR_RANGE)) == 0U;
        health.estimator_healthy =
            (health.active_faults & ADCS_FAULT_ATTITUDE_INVALID) == 0U;
        health.actuators_healthy =
            (health.active_faults & ADCS_FAULT_ACTUATOR) == 0U;
        health.minimum_stack_margin_words = minimum_stack_margin();
        adcs_manager_set_health(&health);
        adcs_manager_update();
        fault_management_pet();
        xTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(HOUSEKEEPING_TASK_PERIOD_MS));
    }
}
