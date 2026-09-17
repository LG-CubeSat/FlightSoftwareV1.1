#include "tasks/sensor_read_task.h"

#include "FreeRTOS.h"
#include "task.h"
#include "thermal_sensor.h"

#include "stdint.h"
#include <stdio.h>

#include "thermal_data.h"

#define SENSOR_1_ADDRESS (0x48u)
#define SENSOR_1_ID (1u)

#define MIN_VALID_TEMPERATURE_C (-40.0f)
#define MAX_VALID_TEMPERATURE_C (125.0f)

#define SENSOR_READ_TASK_PRIORITY (2)
#define SENSOR_READ_TASK_STACK_SIZE (1024)
#define SENSOR_READ_TASK_PERIOD_MS (100)

static StackType_t xSensorReadTaskStack[SENSOR_READ_TASK_STACK_SIZE];
static StaticTask_t xSensorReadTaskBuffer;

TaskHandle_t xSensorReadHandle = NULL;

static thermal_sensor_t sensor;

void sensor_read_task_init(void)
{

    sensor.address = SENSOR_1_ADDRESS;
    sensor.sensor_id = SENSOR_1_ID;

    int sensor_init_status = thermal_sensor_init(&sensor,sensor.address);
    if (sensor_init_status == -1)
    {
        printf("[THERMALS] Sensor initialization failed.\nInvalid Address\n");
        fflush(stdout);
} else if (sensor_init_status == -2){
        printf("[THERMALS] Sensor initialization failed.\nMax sensors already initialized\n");
        fflush(stdout);
}


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
    } else {
        printf("[THERMAL_SENSOR_READ] Initialized successfully.\n");
    }
}


void sensor_read_task(void *pvParameters) {

    (void) pvParameters;

    TickType_t lastWakeTime = xTaskGetTickCount();

 
    static float currentTemp;
    int read_status;

    for (;;) {

        read_status = thermal_sensor_read(&sensor, &currentTemp);

        if (read_status < 0) {
            printf(
                "[THERMALS] ERROR OCCURRED WHILE READING SENSOR\n"
                "SENSOR ADDRESS = %u\n"
                "SENSOR ID = %u\n",
                (unsigned int)sensor.address,
                (unsigned int)sensor.sensor_id
            );
    fflush(stdout);
        }
        else { //executes if sensor read is succsessful

        if (currentTemp < MIN_VALID_TEMPERATURE_C || currentTemp > MAX_VALID_TEMPERATURE_C) {
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
}
