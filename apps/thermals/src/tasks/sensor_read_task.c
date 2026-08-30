#include "../../include/thermal_sensor.h"

#include "FreeRTOS.h"
#include "task.h"

#include "stdint.h"


#define SENSOR_READ_TASK_PRIORITY (1)
#define SENSOR_READ_TASK_STACK_SIZE (1024)
#define SENSOR_READ_TASK_PERIOD_MS (2000)

static StackType_t xSensorReadTaskStack[SENSOR_READ_TASK_STACK_SIZE];
static StaticTask_t xSensorReadTaskBuffer;

TaskHandle_t xSensorReadHandle = NULL;

void sensor_read_task_init(void)
{
    xSensorReadHandle = xTaskCreateStatic(
        sensor_read_task,
        "sensor_read",
        SENSOR_READ_TASK_STACK_SIZE,
        NULL,
        SENSOR_READ_TASK_PRIORITY,
        xSensorReadTaskStack,
        &xSensorReadTaskBuffer
    );

    if (xSensorReadHandle == NULL) {
        printf("[THERMAL_SENSOR_READ] Failed to initialize.\n");
    }
}

void sensor_read_task(void *pvParameters) {

    (void) pvParameters;

    TickType_t lastWakeTime = xTaskGetTickCount();

    for (;;) {

        

        xTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(SENSOR_READ_TASK_PERIOD_MS)
        );
    }

}
