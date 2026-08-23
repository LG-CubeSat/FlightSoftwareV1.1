/*
Sensor Task
fastest task of all (100-200hz)
Loops
Read battery state, and current flow (if possible), and then give estimation task that info or sum.
*/

#include "../../include/tasks/sensor_task.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>

#define SENSOR_TASK_PRIORITY (3)
#define SENSOR_TASK_STACK_SIZE (1024)
#define SENSOR_TASK_PERIOD_MS (100)

static StackType_t xSensorTaskStack[SENSOR_TASK_STACK_SIZE];
static StaticTask_t xSensorTaskBuffer;

TaskHandle_t xSensorHandle = NULL;

void sensor_task_init(void)
{
    xSensorHandle = xTaskCreateStatic(
        sensor_task,
        "sensor",
        SENSOR_TASK_STACK_SIZE,
        NULL,
        SENSOR_TASK_PRIORITY,
        xSensorTaskStack,
        &xSensorTaskBuffer
    );

    if (xSensorHandle == NULL)
    {
        printf("[SENSOR] Failed to initialize\n");
    }
}

void sensor_task(void *pvParameters)
{
    (void) pvParameters;

    TickType_t lastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        // do stuff
        xTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(SENSOR_TASK_PERIOD_MS)
        );
    }
}
