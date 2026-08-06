/*
Estimation Task
Runs 50-100 HZ
waits for sensor packet
runs the kalman filter
publishes the current attitude
*/

#include "../../include/tasks/estimation_task.h"

#include "FreeRTOS.h"
#include "task.h"

#define ESTIMATION_TASK_PRIORITY (5)
#define ESTIMATION_TASK_STACK_SIZE (1024)
#define ESTIMATION_TASK_PERIOD_MS (20)

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

    for (;;)
    {
        // do stuff

        xTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(ESTIMATION_TASK_PERIOD_MS)
        );
    }
}
