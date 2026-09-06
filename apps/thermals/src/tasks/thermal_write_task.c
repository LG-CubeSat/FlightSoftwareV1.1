#include "tasks/thermal_write_task.h"

#include "FreeRTOS.h"
#include "task.h"

#include "thermal_data.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>


#define THERMAL_WRITE_TASK_PRIORITY (1)
#define THERMAL_WRITE_TASK_STACK_SIZE (1024)
#define THERMAL_WRITE_TASK_PERIOD_MS (100)

static StackType_t xThermalWriteTaskStack[THERMAL_WRITE_TASK_STACK_SIZE];
static StaticTask_t xThermalWriteTaskBuffer;

TaskHandle_t xThermalWriteHandle = NULL;

void thermal_write_task_init(void)
{
    xThermalWriteHandle = xTaskCreateStatic(
        thermal_write_task,
        "thermal_write",
        THERMAL_WRITE_TASK_STACK_SIZE,
        NULL,
        THERMAL_WRITE_TASK_PRIORITY,
        xThermalWriteTaskStack,
        &xThermalWriteTaskBuffer
    );

    if (xThermalWriteHandle == NULL) {
        printf("[THERMAL_WRITE_TASK] Failed to initialize.\n");
    }

}

void thermal_write_task(void *pvParameters) {

    (void) pvParameters;

    TickType_t lastWakeTime = xTaskGetTickCount();

    float current_temp;
    float goal_temp;

    for (;;) {

        current_temp = get_thermal_data().current_temp;
        goal_temp = get_thermal_data().goal_temp;

        if (fabs(goal_temp - current_temp) > 1.0f) {

        //to be implimented w/ hardware
        //writes thermals to get closer to goal temp



        }

        xTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(THERMAL_WRITE_TASK_PERIOD_MS)
        );
    }

}
