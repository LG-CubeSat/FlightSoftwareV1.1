#include "tasks/heater_set_task.h"

#include "FreeRTOS.h"
#include "task.h"

#include "thermal_data.h"
#include "prop_int_controller.h"
#include "heater_interface.h"

#include <stdint.h>
#include <stdio.h>

#define HEATER_SET_TASK_PRIORITY (2)
#define HEATER_SET_TASK_STACK_SIZE (1024)
#define HEATER_SET_TASK_PERIOD_MS (100)


/*
 * Temporary simulation gains.
 * TODO(SIM): tune using the completed thermal model.
 * TODO(HW): retune using measured thermal-bed behavior.
 */
#define PI_PROPORTIONAL_GAIN (0.2f)
#define PI_INTEGRAL_GAIN     (0.004f)

#define PI_STEP_SECONDS ((float)HEATER_SET_TASK_PERIOD_MS / 1000.0f)
//basically delta-time

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

    if (!prop_int_controller_init(
        PI_PROPORTIONAL_GAIN,
        PI_INTEGRAL_GAIN))
    {
        force_heater_off();
        printf("[HEATER_SET_TASK] PI controller initialization failed.\n");
        fflush(stdout);
        return;
    }

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

void heater_set_task(void *pvParameters)
{
    (void) pvParameters;

    TickType_t lastWakeTime = xTaskGetTickCount();
    ThermalData_t thermal_data;
    float average_temp;
    float target_temp;
    float requested_power_fraction;
    int control_fault_reported = 0;

    for (;;)
    {
        thermal_data = get_thermal_data();

        if (thermal_data.valid_sensor_mask != 0u &&
            thermal_data.target_temp_valid != 0u)
        {
            average_temp = thermal_data.average_temp;
            target_temp = thermal_data.target_temp;

            if (!prop_int_controller_update(
                    target_temp,
                    average_temp,
                    PI_STEP_SECONDS,
                    &requested_power_fraction) ||
                !heater_set_power(requested_power_fraction))
            {
                prop_int_controller_reset();
                force_heater_off();

                if (control_fault_reported == 0)
                {
                    printf(
                        "[HEATER_SET_TASK] Heater control update failed.\n"
                    );
                    fflush(stdout);
                    control_fault_reported = 1;
                }
            }
            else
            {
                control_fault_reported = 0;
            }
        }
        else
        {
            prop_int_controller_reset();
            force_heater_off();
            control_fault_reported = 0;

            /*
             * TODO(HW): force the physical heater output off through
             * the hardware heater driver.
             */
        }

        xTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(HEATER_SET_TASK_PERIOD_MS)
        );
    }
}
