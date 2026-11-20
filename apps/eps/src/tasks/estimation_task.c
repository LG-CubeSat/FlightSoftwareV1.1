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
#include "pthread.h"

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define ESTIMATION_TASK_PRIORITY (3)
#define ESTIMATION_TASK_STACK_SIZE (1024)
#define ESTIMATION_TASK_PERIOD_MS (100)

static uint32_t previous_sensor_timestamp_ms = 0U;
static uint32_t previous_sensor_sequence = 0U;
static bool has_previous_sensor_sample = false;

static StackType_t xEstimationTaskStack[ESTIMATION_TASK_STACK_SIZE];
static StaticTask_t xEstimationTaskBuffer;

TaskHandle_t xEstimationHandle = NULL;

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

    eps_sensor_data_t local_sensor_data;
    eps_estimation_data_t local_estimation_data;

    for (;;)
    {

        //read data
        local_sensor_data = get_eps_sensor_data();
        local_estimation_data = get_eps_estimation_data();

        //time calculations
        if (!has_previous_sensor_sample) {

            previous_sensor_timestamp_ms = local_sensor_data.timestamp_ms;
            previous_sensor_sequence = local_sensor_data.sequence;
            has_previous_sensor_sample = true;
        }
        else if (local_sensor_data.sequence != previous_sensor_sequence) {

            uint32_t elapsed_ms = local_sensor_data.timestamp_ms - previous_sensor_timestamp_ms;
            float elapsed_hours = (float)elapsed_ms / 3600000.0f;

            previous_sensor_timestamp_ms = local_sensor_data.timestamp_ms;
            previous_sensor_sequence = local_sensor_data.sequence;

            (void)elapsed_hours;
        }

        /*
        calculations (each has to check if data is valid using valid mask in sensor data), if data it's trying to access
        is n/a (a.k.a. invalid) it will return NAN along with printing the error
        note there are no else statements that print the errors as this is a VERY frequent task therefore it would flood the command
        line, in fault handler there will be a section that can check all the valid masks and will deal with not being able to get data
        */

        //eps_solar_power calculation
        if (((local_sensor_data.valid_sensor_mask & EPS_SOLAR_VOLTAGE) != 0u) 
        && ((local_sensor_data.valid_sensor_mask & EPS_SOLAR_CURRENT) != 0u) 
        && (isfinite(local_sensor_data.solar_voltage_reading)) 
        && (isfinite(local_sensor_data.solar_current_reading))) {

            if (isfinite(local_estimation_data.solar_power_w = calculateWatts(local_sensor_data.solar_voltage_reading,local_sensor_data.solar_current_reading))) {
                local_estimation_data.valid_estimation_mask |= SOLAR_POWER_W;
            }  else {

            local_estimation_data.solar_power_w = NAN;
            local_estimation_data.valid_estimation_mask &= ~SOLAR_POWER_W;
        }

        } else {

            local_estimation_data.solar_power_w = NAN;
            local_estimation_data.valid_estimation_mask &= ~SOLAR_POWER_W;
        }

        //eps_battery_power calculation
        if ((local_sensor_data.valid_sensor_mask & EPS_BATTERY_VOLTAGE) 
        && (local_sensor_data.valid_sensor_mask & EPS_BATTERY_CURRENT)
        && (isfinite(local_sensor_data.battery_current_reading))
        && (isfinite(local_sensor_data.battery_voltage_reading))) {

            if (isfinite(local_estimation_data.battery_power_w = calculateWatts(local_sensor_data.battery_voltage_reading,local_sensor_data.battery_current_reading))) {
                local_estimation_data.valid_estimation_mask |= BATTERY_POWER_W;
            }  else {

            local_estimation_data.battery_power_w = NAN;
            local_estimation_data.valid_estimation_mask &= ~BATTERY_POWER_W;
        }

        } else {

            local_estimation_data.battery_power_w = NAN;
            local_estimation_data.valid_estimation_mask &= ~BATTERY_POWER_W;
        }


        //eps_battery_power_balance calculation
        if ((local_estimation_data.valid_estimation_mask & SOLAR_POWER_W)
        && (local_estimation_data.valid_estimation_mask & BATTERY_POWER_W)
        && (isfinite(local_estimation_data.solar_power_w))
        && (isfinite(local_estimation_data.battery_power_w))) {

            if (isfinite(local_estimation_data.estimated_power_balance_w = local_estimation_data.solar_power_w - local_estimation_data.battery_power_w)) {
                local_estimation_data.valid_estimation_mask |= ESTIMATED_POWER_BALANCE;
            }  else {

            local_estimation_data.estimated_power_balance_w = NAN;
            local_estimation_data.valid_estimation_mask &= ~ ESTIMATED_POWER_BALANCE;
        }

        } else {

            local_estimation_data.estimated_power_balance_w = NAN;
            local_estimation_data.valid_estimation_mask &= ~ ESTIMATED_POWER_BALANCE;
        }



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
