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

#define SENSOR_MAX_AGE_MS (200U)
#define MAX_SAMPLE_INTERVAL_MS (300U)

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
    
    bool previous_battery_power_valid = false;

    for (;;)
    {

        //read data
        local_sensor_data = get_eps_sensor_data();
        local_estimation_data = get_eps_estimation_data();

        bool new_sensor_sample =
        (!has_previous_sensor_sample && local_sensor_data.sequence != 0U) ||
        (has_previous_sensor_sample &&
        local_sensor_data.sequence != previous_sensor_sequence);

        //time calculations

        float elapsed_hours = 0.0f;

        if (!has_previous_sensor_sample
            && local_sensor_data.sequence != 0u) {

            previous_sensor_timestamp_ms = local_sensor_data.timestamp_ms;
            previous_sensor_sequence = local_sensor_data.sequence;
            has_previous_sensor_sample = true;
        }
        else if (has_previous_sensor_sample
                 && local_sensor_data.sequence != previous_sensor_sequence) {

            
            uint32_t elapsed_ms = local_sensor_data.timestamp_ms - previous_sensor_timestamp_ms;

            if (elapsed_ms > 0U 
                && elapsed_ms <= MAX_SAMPLE_INTERVAL_MS) {

                elapsed_hours = elapsed_ms / 3600000.0f;
                }

            previous_sensor_timestamp_ms = local_sensor_data.timestamp_ms;
            previous_sensor_sequence = local_sensor_data.sequence;

        }

        uint32_t now_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());

        uint32_t sample_age_ms = now_ms - local_sensor_data.timestamp_ms;

        bool sensor_data_fresh = sample_age_ms <= SENSOR_MAX_AGE_MS;

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
        && (isfinite(local_sensor_data.solar_current_reading))
        && sensor_data_fresh) {

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
        && (isfinite(local_sensor_data.battery_voltage_reading))
        && sensor_data_fresh) {

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

        bool current_battery_power_valid =
        (local_estimation_data.valid_estimation_mask & BATTERY_POWER_W) != 0U;

        bool battery_interval_valid =
        new_sensor_sample &&
        previous_battery_power_valid &&
        current_battery_power_valid &&
        elapsed_hours > 0.0f;


        //eps_state_of_flow calculation
        if (local_estimation_data.battery_power_w > 0.00f) {

            local_estimation_data.state_of_flow = 1; //charging
            local_estimation_data.valid_estimation_mask |= STATE_OF_FLOW; //valid data

        } else if (local_estimation_data.battery_power_w < 0.00f) {

            local_estimation_data.state_of_flow = -1; //discharging
            local_estimation_data.valid_estimation_mask |= STATE_OF_FLOW; //valid data

        } else if (local_estimation_data.battery_power_w == 0.00f) {

            local_estimation_data.state_of_flow = 0; //idle
            local_estimation_data.valid_estimation_mask |= STATE_OF_FLOW; //valid data

        } else {

            local_estimation_data.state_of_flow = 2; //invalid / NaN / N/A
            local_estimation_data.valid_estimation_mask &= ~STATE_OF_FLOW; //data invalid
            
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
        
        //eps_battery_energy_in_wh calculation
        if (battery_interval_valid
            && local_estimation_data.state_of_flow == 1 /*charging*/ 
            && local_estimation_data.valid_estimation_mask & STATE_OF_FLOW
            && local_estimation_data.valid_estimation_mask & BATTERY_POWER_W
            && elapsed_hours > 0.00f) {

            if(isfinite(local_estimation_data.battery_energy_in_wh += calculateEnergy(local_estimation_data.battery_power_w,elapsed_hours))) {
                local_estimation_data.valid_estimation_mask |= BATTERY_ENERGY_IN_WH;
            } else {
                local_estimation_data.valid_estimation_mask &= ~BATTERY_ENERGY_IN_WH;
            }
        } 
        if (new_sensor_sample || !sensor_data_fresh) {
        previous_battery_power_valid = current_battery_power_valid;
        }

        //eps_battery_energy_out_wh calculation
        if (battery_interval_valid
            && local_estimation_data.state_of_flow == -1 /*discharging*/
            && local_estimation_data.valid_estimation_mask & STATE_OF_FLOW
            && local_estimation_data.valid_estimation_mask & BATTERY_POWER_W
            && elapsed_hours > 0.00f) {
                
                if(isfinite(local_estimation_data.battery_energy_out_wh += calculateEnergy(-local_estimation_data.battery_power_w,elapsed_hours))) {

                    local_estimation_data.valid_estimation_mask |= BATTERY_ENERGY_OUT_WH;
                } else {
                    local_estimation_data.valid_estimation_mask &= ~BATTERY_ENERGY_OUT_WH;
                }
        }
        
        if (new_sensor_sample || !sensor_data_fresh) {
        previous_battery_power_valid = current_battery_power_valid;
        }


        /*

        to-do: implement calculations for:

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
