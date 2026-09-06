/*
Housekeeping Task
Maintains basic homeostasis
Stack usage, CPU usage, Temp, Task heartbeat
*/

#include "../../include/tasks/housekeeping_task.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>

#define HOUSEKEEPING_TASK_PRIORTIY (1)
#define HOUSEKEEPING_TASK_STACK_SIZE (1024)
#define HOUSEKEEPING_TASK_PERIOD_MS (1000)

static StackType_t xHousekeepingTaskStack[HOUSEKEEPING_TASK_STACK_SIZE];
static StaticTask_t xHousekeepingTaskBuffer;

TaskHandle_t xHousekeepingHandle = NULL;

void housekeeping_task_init(void)
{
    xHousekeepingHandle = xTaskCreateStatic(
        housekeeping_task,
        "housekeeping",
        HOUSEKEEPING_TASK_STACK_SIZE,
        NULL,
        HOUSEKEEPING_TASK_PRIORTIY,
        xHousekeepingTaskStack,
        &xHousekeepingTaskBuffer
    );

    if (xHousekeepingHandle == NULL)
    {
        printf("[HOUSEKEEPING] Failed to initialize.\n");
    }
}

void housekeeping_task(void *pvParameters)
{
    (void)pvParameters;

    TickType_t lastWakeTime = xTaskGetTickCount();

    for (;;)
    {

        // do stuff

        xTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(HOUSEKEEPING_TASK_PERIOD_MS)
        );
    }
}
