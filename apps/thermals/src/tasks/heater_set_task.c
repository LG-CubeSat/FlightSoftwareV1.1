#include "tasks/heater_set_task.h"

#include "FreeRTOS.h"
#include "task.h"

#include "thermal_data.h"
#include "heater_interface.h"

#include <stdint.h>
#include <stdio.h>

#define HEATER_SET_TASK_PRIORITY (2)
#define HEATER_SET_TASK_STACK_SIZE (1024)
#define HEATER_SET_TASK_PERIOD_MS (100)

#define ERROR_TOLERANCE (1.5f) // Degrees Celsius

static StackType_t xHeaterSetTaskStack[HEATER_SET_TASK_STACK_SIZE];
static StaticTask_t xHeaterSetTaskBuffer;

TaskHandle_t xHeaterSetTask = NULL;

void heater_set_task_init(void)
{
    if (!heater_init()) {

    force_heater_off();
    printf("[THERMALS] Heater driver initialization failed!\n");
    fflush(stdout);
    return;

    }
    
    force_heater_off();

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
        force_heater_off();
        printf("[HEATER_SET_TASK] Failed to initialize.\n");
    } else {
        printf("[HEATER_SET_TASK] Initialized successfully.\n");
    }

}

void heater_set_task(void *pvParameters) {

    (void) pvParameters;

    TickType_t lastWakeTime = xTaskGetTickCount();

    ThermalData_t thermal_data;

    float average_temp;
    float target_temp;

    for (;;) {

        thermal_data = get_thermal_data();

        if (thermal_data.valid_sensor_mask != 0u && thermal_data.target_temp_valid != 0u) 
        // is at least one sensor active and is the target temp valid?
        {
            average_temp = thermal_data.average_temp;
            target_temp = thermal_data.target_temp;

            if ((target_temp - average_temp) > ERROR_TOLERANCE)
            {
                force_heater_off();

                /*
                * TODO(SIM): replace this forced-off placeholder with
                * PI-controller output.
                */
            }
            else {
                force_heater_off();
            }
        }
        else
        {
            force_heater_off();
            /*
            * TODO(HW): the hardware heater driver must force the
            * physical heater output off here.
            */
        }

        xTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(HEATER_SET_TASK_PERIOD_MS)
        );
    }

}
