/*
Estimation Task
Tases information from sensor and stuff, then does math on it to calculate all needed statistics.
I'm pretty sure the fault manager will handle what to do when there is a mistake.
*/

#include "../../include/tasks/estimation_task.h"

#include "FreeRTOS.h"
#include "task.h"

#include "eps_data.h"
#include "powerCalculations.h"

#include <stdio.h>
#include <stdint.h>

#define ESTIMATION_TASK_PRIORITY (3)
#define ESTIMATION_TASK_STACK_SIZE (1024)
#define ESTIMATION_TASK_PERIOD_MS (100)

static StackType_t xEstimationTaskStack[ESTIMATION_TASK_STACK_SIZE];
static StaticTask_t xEstimationTaskBuffer;

TaskHandle_t xEstimationHandle = NULL;

eps_sensor_data_t local_sensor_data;
eps_estimation_data_t local_estimation_data;

void estimation_task_init(void)
{
    xEstimationHandle = xTaskCreateStatic(
        estimation_task,
        "estimation",
        ESTIMATION_TASK_STACK_SIZE,
        NULL,
        ESTIMATION_TASK_PRIORITY,
        xEstimationTaskStack,
        &xEstimationTaskBuffer
    );

    if (xEstimationHandle == NULL)
    {
        printf("[ESTIMATION] Failed to initialize.\n");
    }

}

void estimation_task(void *pvParameters)
{
    (void) pvParameters;

    TickType_t lastWakeTime = xTaskGetTickCount();

    for (;;)
    {

        //read data
        local_sensor_data = get_eps_sensor_data();
        local_estimation_data = get_eps_estimation_data();

        //calculations
        local_estimation_data.solar_power_w = calculateWatts(local_sensor_data.solar_voltage_reading,local_sensor_data.solar_current_reading);
        local_estimation_data.battery_power_w = calculateWatts(local_sensor_data.battery_voltage_reading,local_sensor_data.battery_current_reading);
        local_estimation_data.estimated_power_balance_w = local_estimation_data.solar_power_w - local_estimation_data.battery_power_w;
        local_estimation_data.battery_energy_in_wh = local_estimation_data.battery_
        /*

        to-do: implement calculations for:

        battery_energy_in_wh;
        battery_energy_out_wh;
        estimated_state_of_charge_pct;
        average_solar_power_w;
        average_battery_current_a;
        estimated_time_to_empty_h;
        estimated_time_to_full_h;
    
        */

        //set data
        set_eps_estimation_data(local_estimation_data);

        uint32_t notified_value;
        if (xTaskNotifyWait(0, 0, &notified_value, 0) == pdTRUE)
        {
            int32_t target_position = (int32_t)notified_value;
            printf("[ESTIMATION] Updating attitude estimate for target position: %d\n", target_position);
            fflush(stdout);
        }

        xTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(ESTIMATION_TASK_PERIOD_MS)
        );
    }
}
