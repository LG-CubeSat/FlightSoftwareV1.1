#include "tasks/heater_set_task.h"

#include "FreeRTOS.h"
#include "task.h"

#include "thermal_data.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>


#define HEATER_SET_TASK_PRIORITY (2)
#define HEATER_SET_TASK_STACK_SIZE (1024)
#define HEATER_SET_TASK_PERIOD_MS (100)

#define ERROR_TOLERANCE (1.5) //degrees C

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

    float current_temp;
    float target_temp;

    for (;;) {

        current_temp = get_thermal_data().current_temp;
        target_temp = get_thermal_data().target_temp;

        if (fabs(target_temp - current_temp) > ERROR_TOLERANCE) {

        //to be implimented w/ hardware
        //sets heater to get closer to goal temp



        }

        xTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(HEATER_SET_TASK_PERIOD_MS)
        );
    }

}
