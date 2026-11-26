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

        /*
        For V1 right now, the fault manager doesn't actually do anything, it just stores data on the state and defines
        critical errors along with warnings and whatnot, eventually in late V1 or V2, the fault manager will send it's data over 
        to obc


        V2 will implement much more data, right now there is no way of telling what put the system into a current state except for
        potentially our current errors mask but it's not very accurate, there will be more variables and more trips for the system to set
        off; this version is mainly focused on hw implementation so most of the fault manager changes are going to happen in v3

        V3 is going to be by far the biggest change for fault manager, instead of just storing data, we will have the avaliable hw
        to actually do stuff with that data. For example, if the voltage is too high, a path the fault manager might take is: 
        sensor task checks voltage -> fault manager reads voltage too high -> fault manager sets state to critical and flags voltage as
        too high -> fault manager executes a new file / function that determines what to do -> that file/function will likely turn off
        power to that rail for a breif moment or do something else to change the voltage. That's just one example and I'm going to try and
        get it to look something like that.

        V4 is going to be overall bug fixing and sweeping along with any refinements that need to be made. I'm going to try to add a feature 
        where obc can send commands, not necessarily to fault manager, but a seperate file that will have plently of commands related to fault
        manager. Now that I think about it, there is probably going to be a completely new task that controls hardware for obc commands and
        for fault manager shutoffs. And the final most advanced update to fault manager will be the energy allocation, this will be done with
        sw and hw and it basically just makes sure everything is getting enough power and not too much/too little. I thknk I'm going to make a 
        completely new task for this for watching everyhting, but it will have heavy integration with fault manager 
        */

        //get data
        eps_estimation_data_t local_estimation_data = get_eps_estimation_data();
        eps_sensor_data_t local_sensor_data = get_eps_sensor_data();
        eps_fault_data_t local_fault_data = get_eps_fault_data();

        uint32_t now_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());

        enum eps_state state_before_checks = local_fault_data.current_state;

        if (isfinite(local_sensor_data.battery_voltage_reading) &&
            local_sensor_data.valid_sensor_mask & EPS_BATTERY_VOLTAGE  ) {

            if (local_sensor_data.battery_voltage_reading > CRITICAL_BATTERY_VOLTAGE_HIGH) {

                local_fault_data.active_faults |= BATT_HIGH_VOLTAGE;

            } else {

                local_fault_data.active_faults &= ~BATT_HIGH_VOLTAGE;
            }

            if (local_sensor_data.battery_voltage_reading > WARNING_BATTERY_VOLTAGE_HIGH) {

                local_fault_data.active_warnings |= BATT_HIGH_VOLTAGE;

            } else {

                local_fault_data.active_warnings &= ~BATT_HIGH_VOLTAGE;
            } 

            if (local_sensor_data.battery_voltage_reading < CRITICAL_BATTERY_VOLTAGE_LOW) {

                local_fault_data.active_faults |= BATT_LOW_VOLTAGE;

            } else {

                local_fault_data.active_faults &= ~BATT_LOW_VOLTAGE;
            }

            if (local_sensor_data.battery_voltage_reading < WARNING_BATTERY_VOLTAGE_LOW) {

                local_fault_data.active_warnings |= BATT_LOW_VOLTAGE;

            } else {

                local_fault_data.active_warnings &= ~BATT_LOW_VOLTAGE;
            } 



            }

            if (isfinite(local_sensor_data.battery_current_reading) &&
            local_sensor_data.valid_sensor_mask & EPS_BATTERY_CURRENT  ) {

            if (local_sensor_data.battery_current_reading > CRITICAL_BATTERY_CURRENT_HIGH) {

                local_fault_data.active_faults |= BATT_HIGH_CURRENT;

            } else {

                local_fault_data.active_faults &= ~BATT_HIGH_CURRENT;
            }

            if (local_sensor_data.battery_voltage_reading > WARNING_BATTERY_CURRENT_HIGH) {

                local_fault_data.active_warnings |= BATT_HIGH_CURRENT;

            } else {

                local_fault_data.active_warnings &= ~BATT_HIGH_CURRENT;
            } 

            if (local_sensor_data.battery_current_reading < CRITICAL_BATTERY_CURRENT_LOW) {

                local_fault_data.active_faults |= BATT_LOW_CURRENT;

            } else {

                local_fault_data.active_faults &= ~BATT_LOW_CURRENT;
            }

            if (local_sensor_data.battery_voltage_reading > WARNING_BATTERY_CURRENT_LOW) {

                local_fault_data.active_warnings |= BATT_LOW_CURRENT;

            } else {

                local_fault_data.active_warnings &= ~BATT_LOW_CURRENT;
            } 

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
