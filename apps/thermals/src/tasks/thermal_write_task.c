#include "../../include/thermal_sensor.h"

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"

#include "math.h"
#include "stdint.h"


#define THERMAL_WRITE_TASK_PRIORITY (1)
#define THERMAL_WRITE_TASK_STACK_SIZE (1024)
#define THERMAL_WRITE_TASK_PERIOD_MS (500)

static StackType_t xThermalWriteTaskStack[THERMAL_WRITE_TASK_STACK_SIZE];
static StaticTask_t xThermalWriteTaskBuffer;

TaskHandle_t xSensorReadHandle = NULL;

void thermal_write_task_init(void)
{
    xSensorReadHandle = xTaskCreateStatic(
        thermal_write_task,
        "thermal_write",
        THERMAL_WRITE_TASK_STACK_SIZE,
        NULL,
        THERMAL_WRITE_TASK_PRIORITY,
        xThermalWriteTaskStack,
        &xThermalWriteTaskBuffer
    );

    if (xSensorReadHandle == NULL) {
        printf("[THERMAL_WRITE_TASK] Failed to initialize.\n");
    }
}

void thermal_write_task(void *pvParameters) {

    (void) pvParameters;

    TickType_t lastWakeTime = xTaskGetTickCount();

    for (;;) {

        //checks if current temp is more than one degree away from goal temp (for sensor 1) *repeat for sensor 2
        while (fabs(goal_temp1 - temperature1) > 1) {

            /* 
            send signal to increase or decrease temperature using onboard heating strategy 
            (set power of heater proportional to difference from current and goal)
            then send semaphore to wake up thermal_read task and block this current task until a new value is detected from thermal_read 
             */

        }

        xTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(THERMAL_WRITE_TASK_PERIOD_MS)
        );
    }

}
