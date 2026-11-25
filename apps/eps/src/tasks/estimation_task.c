/*
Estimation Task
Tases information from sensor and stuff, then does math on it to calculate all needed statistics.
I'm pretty sure the fault manager will handle what to do when there is a mistake.
*/

#include "../../include/tasks/estimation_task.h"

#include "FreeRTOS.h"
#include "task.h"

#include <inttypes.h>

#include "eps_data.h"
#include "powerCalculations.h"
#include "eps_constants.h"
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
    
    bool previous_battery_power_valid = false;
    bool battery_energy_history_complete = true;

    float solar_energy_for_average_wh = 0.0f;
    float solar_average_duration_h = 0.0f;
    bool previous_solar_power_valid = false;

    float battery_current_time_ah = 0.0f;
    float battery_current_average_duration_h = 0.0f;
    bool previous_battery_current_valid = false;

#if EPS_DEBUG_OUTPUT
    uint8_t debug_counter = 0;
#endif

    for (;;)
    {

        //read data
        local_sensor_data = get_eps_sensor_data();
        local_estimation_data = get_eps_estimation_data();


        bool new_sensor_sample =
        (!has_previous_sensor_sample && local_sensor_data.sequence != 0U) ||
        (has_previous_sensor_sample &&
        local_sensor_data.sequence != previous_sensor_sequence);

        bool interval_expected = has_previous_sensor_sample && new_sensor_sample;

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
        is n/a (a.k.a. invalid) it will return NAN
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

        if (interval_expected && !battery_interval_valid) {
            battery_energy_history_complete = false;
            }

        if (has_previous_sensor_sample && !sensor_data_fresh) {
             battery_energy_history_complete = false;
            }


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

            float next_energy_in = local_estimation_data.battery_energy_in_wh + calculateEnergy(local_estimation_data.battery_power_w,elapsed_hours);   

            if(isfinite(next_energy_in)) {
                local_estimation_data.battery_energy_in_wh = next_energy_in;
                local_estimation_data.valid_estimation_mask |= BATTERY_ENERGY_IN_WH;
            } else {
                battery_energy_history_complete = false;
                local_estimation_data.valid_estimation_mask &= ~BATTERY_ENERGY_IN_WH;
            }
        } 

        //eps_battery_energy_out_wh calculation
        if (battery_interval_valid
            && local_estimation_data.state_of_flow == -1 /*discharging*/
            && local_estimation_data.valid_estimation_mask & STATE_OF_FLOW
            && local_estimation_data.valid_estimation_mask & BATTERY_POWER_W
            && elapsed_hours > 0.00f) {
                
                float next_energy_out = local_estimation_data.battery_energy_out_wh + calculateEnergy(-local_estimation_data.battery_power_w,elapsed_hours);
                

                if(isfinite(next_energy_out)) {

                    local_estimation_data.battery_energy_out_wh = next_energy_out;
                    local_estimation_data.valid_estimation_mask |= BATTERY_ENERGY_OUT_WH;
                } else {
                    battery_energy_history_complete = false;
                    local_estimation_data.valid_estimation_mask &= ~BATTERY_ENERGY_OUT_WH;
                }
        }

        // average_solar_power_w calculation
        if (new_sensor_sample &&
            sensor_data_fresh &&
            previous_solar_power_valid &&
            (local_estimation_data.valid_estimation_mask & SOLAR_POWER_W) != 0U &&
            elapsed_hours > 0.0f) {

            float next_solar_energy_wh = solar_energy_for_average_wh + local_estimation_data.solar_power_w * elapsed_hours;

            float next_solar_duration_h = solar_average_duration_h + elapsed_hours;

            if (isfinite(next_solar_energy_wh) &&
                isfinite(next_solar_duration_h) &&
                next_solar_duration_h > 0.0f) {

                float next_average_w =
                    next_solar_energy_wh / next_solar_duration_h;

                if (isfinite(next_average_w)) {
                    // Save all three results only after checks pass.
                    solar_energy_for_average_wh = next_solar_energy_wh;
                    solar_average_duration_h = next_solar_duration_h;

                    local_estimation_data.average_solar_power_w = next_average_w;
                    local_estimation_data.valid_estimation_mask |=
                        AVERAGE_SOLAR_POWER_W;
                } else {
                    local_estimation_data.average_solar_power_w = NAN;
                    local_estimation_data.valid_estimation_mask &=
                        ~AVERAGE_SOLAR_POWER_W;
                }
            } else {
                local_estimation_data.average_solar_power_w = NAN;
                local_estimation_data.valid_estimation_mask &=
                    ~AVERAGE_SOLAR_POWER_W;
            }
        }

        if (!sensor_data_fresh || (local_estimation_data.valid_estimation_mask & SOLAR_POWER_W) == 0U) {

        local_estimation_data.average_solar_power_w = NAN;
        local_estimation_data.valid_estimation_mask &= ~AVERAGE_SOLAR_POWER_W;
        }
        
        // average_battery_current_a calculation
        bool current_battery_current_valid =
        sensor_data_fresh &&
        (local_sensor_data.valid_sensor_mask & EPS_BATTERY_CURRENT) != 0U &&
        isfinite(local_sensor_data.battery_current_reading);

        if (new_sensor_sample &&
            previous_battery_current_valid &&
            current_battery_current_valid &&
            elapsed_hours > 0.0f) {

            float next_current_time_ah =
                battery_current_time_ah
                + local_sensor_data.battery_current_reading * elapsed_hours;

            float next_duration_h =
                battery_current_average_duration_h + elapsed_hours;

            if (isfinite(next_current_time_ah) &&
                isfinite(next_duration_h) &&
                next_duration_h > 0.0f) {

                float next_average_a = next_current_time_ah / next_duration_h;

                if (isfinite(next_average_a)) {
                    battery_current_time_ah = next_current_time_ah;
                    battery_current_average_duration_h = next_duration_h;

                    local_estimation_data.average_battery_current_a =
                        next_average_a;

                    local_estimation_data.valid_estimation_mask |=
                        AVERAGE_BATTERY_CURRENT_A;
                } else {
                    local_estimation_data.average_battery_current_a = NAN;
                    local_estimation_data.valid_estimation_mask &=
                        ~AVERAGE_BATTERY_CURRENT_A;
                }
            } else {
                local_estimation_data.average_battery_current_a = NAN;
                local_estimation_data.valid_estimation_mask &=
                    ~AVERAGE_BATTERY_CURRENT_A;
            }
        }

        if (!current_battery_current_valid) {
            local_estimation_data.average_battery_current_a = NAN;
            local_estimation_data.valid_estimation_mask &=
                ~AVERAGE_BATTERY_CURRENT_A;
        }

        // Save previous-reading validity after all interval calculations.
        if (new_sensor_sample || !sensor_data_fresh) {
            previous_battery_power_valid = current_battery_power_valid;

            previous_solar_power_valid =
                (local_estimation_data.valid_estimation_mask & SOLAR_POWER_W) != 0U;

            previous_battery_current_valid = current_battery_current_valid;
        }


        if (!battery_energy_history_complete) {
        local_estimation_data.valid_estimation_mask &= ~(BATTERY_ENERGY_IN_WH | BATTERY_ENERGY_OUT_WH); 
        }

        // Default to invalid unless all SOC checks pass.
        local_estimation_data.estimated_state_of_charge_pct = NAN;
        local_estimation_data.valid_estimation_mask &= ~ESTIMATED_STATE_OF_CHARGE_PCT;

        local_estimation_data.estimated_time_to_empty_h = NAN;
        local_estimation_data.valid_estimation_mask &= ~ESTIAMTED_TIME_TO_EMPTY_H;

        local_estimation_data.estimated_time_to_full_h = NAN;
        local_estimation_data.valid_estimation_mask &= ~ESTIMATED_TIME_TO_FULL_H;

        //estimated_state_of_charge_pct
        if (isfinite(BATTERY_USABLE_CAPACITY_WH) &&
            isfinite(BATTERY_INITIAL_ENERGY_WH) &&
            BATTERY_USABLE_CAPACITY_WH > 0.0f &&
            BATTERY_INITIAL_ENERGY_WH >= 0.0f &&
            BATTERY_INITIAL_ENERGY_WH <= BATTERY_USABLE_CAPACITY_WH &&
            battery_energy_history_complete &&
            has_previous_sensor_sample &&
            current_battery_power_valid) {

            float remaining_energy_wh =
                BATTERY_INITIAL_ENERGY_WH
                + local_estimation_data.battery_energy_in_wh
                - local_estimation_data.battery_energy_out_wh;

            if (isfinite(remaining_energy_wh) &&
                remaining_energy_wh >= 0.0f &&
                remaining_energy_wh <= BATTERY_USABLE_CAPACITY_WH) {

                float next_soc_pct =
                    100.0f * (remaining_energy_wh / BATTERY_USABLE_CAPACITY_WH);

                if (isfinite(next_soc_pct)) {
                    local_estimation_data.estimated_state_of_charge_pct =
                        next_soc_pct;

                    local_estimation_data.valid_estimation_mask |=
                        ESTIMATED_STATE_OF_CHARGE_PCT;

                    if (local_estimation_data.battery_power_w <
                        -MIN_DISCHARGE_POWER_W) {

                        float next_time_to_empty_h =
                            remaining_energy_wh
                            / -local_estimation_data.battery_power_w;

                        if (isfinite(next_time_to_empty_h) &&
                            next_time_to_empty_h >= 0.0f) {

                            local_estimation_data.estimated_time_to_empty_h =
                                next_time_to_empty_h;

                            local_estimation_data.valid_estimation_mask |=
                                ESTIAMTED_TIME_TO_EMPTY_H;
                        }

                    } else if (local_estimation_data.battery_power_w >
                               MIN_CHARGE_POWER_W) {

                        float next_time_to_full_h =
                            (BATTERY_USABLE_CAPACITY_WH - remaining_energy_wh)
                            / local_estimation_data.battery_power_w;

                        if (isfinite(next_time_to_full_h) &&
                            next_time_to_full_h >= 0.0f) {

                            local_estimation_data.estimated_time_to_full_h =
                                next_time_to_full_h;

                            local_estimation_data.valid_estimation_mask |=
                                ESTIMATED_TIME_TO_FULL_H;
                        }
                    }
                }
            }
        }



        //set data
        set_eps_estimation_data(local_estimation_data);

#if EPS_DEBUG_OUTPUT
        if (++debug_counter >= 5U) {
            debug_counter = 0U;

            printf(
            "\n** [EPS DATA] **\n"
            "-------------------\n"
            "    SENSOR DATA\n"
            "-------------------\n"
            "Solar Voltage: %.3f V\n"
            "Solar Shunt Voltage: %.3f V\n"
            "Solar Current: %.3f A\n"
            "Battery Voltage: %.3f V\n"
            "Battery Shunt Voltage: %.3f V\n"
            "Battery Current: %.3f A\n"
            "Battery Temperature: %.2f °C\n"
            "Valid Sensor Mask: %" PRIu32 "\n"
            "Timestamp: %" PRIu32 " ms\n"
            "Sequence: %" PRIu32 "\n"
            "-------------------\n"
            "  ESTIMATION DATA\n"
            "-------------------\n"
            "Solar Power: %.3f W\n"
            "Battery Power: %.3f W\n"
            "Estimated Power Balance: %.3f W\n"
            "Battery Energy In: %.6f Wh\n"
            "Battery Energy Out: %.6f Wh\n"
            "Estimated State of Charge: %.3f %%\n"
            "Average Solar Power: %.3f W\n"
            "Average Battery Current: %.3f A\n"
            "Estimated Time to Empty: %.0f h %.0f min %.0f s\n"
            "Estimated Time to Full: %.0f h %.0f min %.0f s\n"
            "State of Flow: %" PRId8
            " (1=charging, -1=discharging, 0=idle, 2=invalid)\n"
            "Valid Estimation Mask: %" PRIu32 "\n"
            "-------------------\n",

            local_sensor_data.solar_voltage_reading,
            local_sensor_data.solar_shunt_voltage_reading,
            local_sensor_data.solar_current_reading,
            local_sensor_data.battery_voltage_reading,
            local_sensor_data.battery_shunt_voltage_reading,
            local_sensor_data.battery_current_reading,
            local_sensor_data.battery_temperature_reading,
            local_sensor_data.valid_sensor_mask,
            local_sensor_data.timestamp_ms,
            local_sensor_data.sequence,

            local_estimation_data.solar_power_w,
            local_estimation_data.battery_power_w,
            local_estimation_data.estimated_power_balance_w,
            local_estimation_data.battery_energy_in_wh,
            local_estimation_data.battery_energy_out_wh,
            local_estimation_data.estimated_state_of_charge_pct,
            local_estimation_data.average_solar_power_w,
            local_estimation_data.average_battery_current_a,
            floor(local_estimation_data.estimated_time_to_empty_h),
            floor(fmod((double)local_estimation_data.estimated_time_to_empty_h * 60.0, 60.0)),
            floor(fmod((double)local_estimation_data.estimated_time_to_empty_h * 3600.0, 60.0)),
            floor(local_estimation_data.estimated_time_to_full_h),
            floor(fmod((double)local_estimation_data.estimated_time_to_full_h * 60.0, 60.0)),
            floor(fmod((double)local_estimation_data.estimated_time_to_full_h * 3600.0, 60.0)),
            local_estimation_data.state_of_flow,
            local_estimation_data.valid_estimation_mask
        );

        fflush(stdout);
        }
#endif

        xTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(ESTIMATION_TASK_PERIOD_MS)
        );
    }
}
