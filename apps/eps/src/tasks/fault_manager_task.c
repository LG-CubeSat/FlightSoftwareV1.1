/*
 * Detect issues with:
 *  overcurrent in a subsystem
 *  battery overheating
 *  solar panel failure
 *
 * automatically:
 *  switch off faulty shit 
 *  reroute power when needed
 * handle turning on safe mode (auto turning off power in specfic systems)
 *  (maybe make it so all the 3 computers can handle safe mode just incase eps dies)
*/

/* Collect one EPS sensor snapshot every 100 ms (10 Hz). */

#include "tasks/fault_manager_task.h"

#include "FreeRTOS.h"
#include "task.h"

#include "eps_data.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "eps_constants.h"

#define FAULT_MANAGER_TASK_PRIORITY (3)
#define FAULT_MANAGER_TASK_STACK_SIZE (1024)
#define FUALT_MANAGER_TASK_PERIOD_MS (300)

static StackType_t xFaultManagerTaskStack[FAULT_MANAGER_TASK_STACK_SIZE];
static StaticTask_t xFaultManagerTaskBuffer;

TaskHandle_t xFaultManagerTaskHandle = NULL;


void fault_manager_task_init(void)
{
    xFaultManagerTaskHandle = xTaskCreateStatic(
        fault_manager_task,
        "fault",
        FAULT_MANAGER_TASK_STACK_SIZE,
        NULL,
        FAULT_MANAGER_TASK_PRIORITY,
        xFaultManagerTaskStack,
        &xFaultManagerTaskBuffer
    );


    if (xFaultManagerTaskHandle == NULL)
    {
        printf("[FAULT MANAGER] Failed to initialize\n");
        fflush(stdout);
    }
    else {
        printf("[FAULT MANAGER] Successfully initialized\n");
        fflush(stdout);
    }
}

void fault_manager_task(void *pvParameters)
{
    (void) pvParameters;

    TickType_t lastWakeTime = xTaskGetTickCount();

    uint32_t state_started_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());


    for (;;)
    {

        //get data
        eps_estimation_data_t local_estimation_data = get_eps_estimation_data();
        eps_sensor_data_t local_sensor_data = get_eps_sensor_data();
        eps_fault_data_t local_fault_data = get_eps_fault_data();

        uint32_t now_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());

        enum eps_state state_before_checks = local_fault_data.current_state;

        
        if (local_sensor_data.battery_voltage_reading < WARNING_BATTERY_VOLTAGE_LOW
        ||  local_sensor_data.battery_voltage_reading > WARNING_BATTERY_VOLTAGE_HIGH
        ||  local_sensor_data.battery_current_reading < WARNING_BATTERY_CURRENT_LOW
        ||  local_sensor_data.battery_current_reading > WARNING_BATTERY_CURRENT_HIGH) {

            local_fault_data.current_state = EPS_STATE_WARNING;

        }



        if (local_fault_data.current_state != state_before_checks) {

            local_fault_data.previous_state = state_before_checks;
            state_started_ms = now_ms;
        }

        local_fault_data.time_spent_in_current_state_ms = now_ms - state_started_ms;

        //set data
        set_eps_fault_data(local_fault_data);

        xTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(FUALT_MANAGER_TASK_PERIOD_MS)
        );
    }
}
