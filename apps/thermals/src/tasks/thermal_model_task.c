/*
 * Periodically advances the software-only thermal environment.
 * TODO(HW): do not create this task in the STM32 hardware target.
 */

#include "tasks/thermal_model_task.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>

#include "heater_interface.h"
#include "simulation/thermal_model.h"

#define THERMAL_MODEL_TASK_PRIORITY        (2)
#define THERMAL_MODEL_TASK_STACK_SIZE      (1024)
#define THERMAL_MODEL_TASK_PERIOD_MS       (100)
#define THERMAL_MODEL_STEP_SECONDS         (0.1f)

static StackType_t
    xThermalModelTaskStack[THERMAL_MODEL_TASK_STACK_SIZE];

static StaticTask_t xThermalModelTaskBuffer;

TaskHandle_t xThermalModelHandle = NULL;

void thermal_model_task_init(void)
{
    if (!thermal_model_init())
    {
        force_heater_off();
        printf("[THERMAL_MODEL] Model initialization failed.\n");
        fflush(stdout);
        return;
    }

    xThermalModelHandle = xTaskCreateStatic(
        thermal_model_task,
        "thermal_model",
        THERMAL_MODEL_TASK_STACK_SIZE,
        NULL,
        THERMAL_MODEL_TASK_PRIORITY,
        xThermalModelTaskStack,
        &xThermalModelTaskBuffer
    );

    if (xThermalModelHandle == NULL)
    {
        force_heater_off();
        printf("[THERMAL_MODEL] Task creation failed.\n");
        fflush(stdout);
    }
    else
    {
        printf("[THERMAL_MODEL] Initialized successfully.\n");
        fflush(stdout);
    }
}

void thermal_model_task(void *pvParameters)
{
    (void)pvParameters;

    TickType_t lastWakeTime = xTaskGetTickCount();

    int fault_reported = 0;

    for (;;)
    {
        float heater_power = get_applied_power();

        if (!thermal_model_step(
                heater_power,
                THERMAL_MODEL_STEP_SECONDS))
        {
            force_heater_off();

            if (fault_reported == 0)
            {
                printf("[THERMAL_MODEL] Model update failed.\n");
                fflush(stdout);
                fault_reported = 1;
            }
        }
        else
        {
            fault_reported = 0;
        }

        xTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(THERMAL_MODEL_TASK_PERIOD_MS)
        );
    }
}