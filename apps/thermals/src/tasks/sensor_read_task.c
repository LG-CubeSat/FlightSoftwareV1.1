#include "tasks/sensor_read_task.h"

#include "FreeRTOS.h"
#include "task.h"
#include "thermal_sensor.h"

#include "stdint.h"
#include <stdio.h>

#include "thermal_data.h"


#define SENSOR_READ_TASK_PRIORITY (1)
#define SENSOR_READ_TASK_STACK_SIZE (1024)
#define SENSOR_READ_TASK_PERIOD_MS (100)

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


    //Placeholder address for sensors, fill in once we get the real hardware
    thermal_sensor_t sensor;
    sensor.ADDRESS = 0x48;
    sensor.SENSOR_GENERIC = 1;

    int sensor_init_value = thermal_sensor_init(&sensor, sensor.ADDRESS);
    if (sensor_init_value == -1) {
        printf("INVALID ADDRESS\n");
    } else if (sensor_init_value == -2) {
        printf("1 SENSORS ALREADY INITIALIZED\n");
    } else if (sensor_init_value == 1) {
        printf("SENSOR INITIALIZED SUCCESSFULLY\n");
        fflush(stdout);
    }

 
    float currentTemp = get_thermal_data().current_temp;

    for (;;) {

        currentTemp = thermal_sensor_read(&sensor);

        if (currentTemp < -40.0 || currentTemp > 125.0) {
            printf("[THERMAL_SENSOR_READ] ERROR: Likely Invalid temperature reading: %.2f°C\n", currentTemp);
            fflush(stdout);
        } else {
            // Update the global thermal data structure with the new temperature reading
            thermals_set_current(currentTemp);
        }

        xTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(SENSOR_READ_TASK_PERIOD_MS)
        );
    }

}
