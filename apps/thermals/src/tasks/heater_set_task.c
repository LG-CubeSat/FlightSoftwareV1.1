#include "tasks/heater_set_task.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>

#define HEATER_SET_TASK_PRIORITY (2)
#define HEATER_SET_TASK_STACK_SIZE (1024)
#define HEATER_SET_TASK_PERIOD_MS (100)


static StackType_t xHeaterSetTaskStack[HEATER_SET_TASK_STACK_SIZE];
static StaticTask_t xHeaterSetTaskBuffer;

TaskHandle_t xHeaterSetTask = NULL;

void heater_set_task_init(void)
{
    xHeaterSetTask = xTaskCreateStatic(
        heater_set_task,
        "heater_set",
        HEATER_SET_TASK_STACK_SIZE,
        NULL,
        HEATER_SET_TASK_PRIORITY,
        xHeaterSetTaskStack,
        &xHeaterSetTaskBuffer
    );

    if (xHeaterSetTask == NULL) {
        printf("[HEATER_SET_TASK] Failed to initialize.\n");
    } else {
        printf("[HEATER_SET_TASK] Initialized successfully.\n");
    }

}

void heater_set_task(void *pvParameters) {

    (void) pvParameters;

    TickType_t lastWakeTime = xTaskGetTickCount();


    for (;;) {

        // heater control remains disabled until the controller and heater interface are implemented.

        xTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(HEATER_SET_TASK_PERIOD_MS)
        );
    }

}
