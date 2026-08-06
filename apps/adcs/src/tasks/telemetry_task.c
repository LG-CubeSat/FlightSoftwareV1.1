/*
Telemetry Task
1-10HZ
Collects everything
Sends out data using CSP
*/

#include "../../include/tasks/telemetry_task.h"

#include "FreeRTOS.h"
#include "task.h"

#define TELEMETRY_TASK_PRIORITY (1)
#define TELEMETRY_TASK_STACK_SIZE (1024)
#define TELEMETRY_TASK_PERIOD_MS (1000)

static StackType_t xTelemetryTaskStack[TELEMETRY_TASK_STACK_SIZE];
static StaticTask_t xTelemetryTaskBuffer;

TaskHandle_t xTelemetryHandle = NULL;

void telemetry_task_init(void)
{
    xTelemetryHandle = xTaskCreateStatic(
        telemetry_task,
        "telemetry",
        TELEMETRY_TASK_STACK_SIZE,
        NULL,
        TELEMETRY_TASK_PRIORITY,
        xTelemetryTaskStack,
        &xTelemetryTaskBuffer
    );

    if (xTelemetryHandle == NULL) {
        printf("[TELEMTRY] Failed to initialize.\n");
    }
}

void telemtry_task(void *pvParameters)
{
    (void) pvParameters;

    TickType_t lastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        // do some stuff

        xTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(TELEMETRY_TASK_PERIOD_MS)
        );
    }
}