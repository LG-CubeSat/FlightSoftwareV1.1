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

#include "manager/fault_manager.h"

#include "FreeRTOS.h"
#include "task.h"

#include "eps_data.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "conversions.h"

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


    for (;;)
    {
        
        if ()



        xTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(FUALT_MANAGER_TASK_PERIOD_MS)
        );
    }
}
