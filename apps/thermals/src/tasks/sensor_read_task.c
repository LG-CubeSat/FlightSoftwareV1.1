#include "tasks/sensor_read_task.h"

#include "FreeRTOS.h"
#include "task.h"
#include "thermal_sensor.h"

#include "stdint.h"
#include <stdio.h>

#include "thermal_data.h"

#define SENSOR_1_ADDRESS (0x48u)
#define SENSOR_2_ADDRESS (0x49u)

#define SENSOR_1_ID (1u)
#define SENSOR_2_ID (2u)

static thermal_sensor_t sensors[MAX_SENSORS]; //sensors is an array that contains a list of sensors mainly for init

#define MIN_VALID_TEMPERATURE_C (-40.0f)
#define MAX_VALID_TEMPERATURE_C (125.0f)

#define SENSOR_READ_TASK_PRIORITY (2)
#define SENSOR_READ_TASK_STACK_SIZE (1024)
#define SENSOR_READ_TASK_PERIOD_MS (100)

static StackType_t xSensorReadTaskStack[SENSOR_READ_TASK_STACK_SIZE];
static StaticTask_t xSensorReadTaskBuffer;

TaskHandle_t xSensorReadHandle = NULL;

void sensor_read_task_init(void)
{

    sensors[0].address = SENSOR_1_ADDRESS;
    sensors[0].sensor_id = SENSOR_1_ID;

    sensors[1].address = SENSOR_2_ADDRESS;
    sensors[1].sensor_id = SENSOR_2_ID;

    for (uint8_t i = 0; i < MAX_SENSORS; i++) {
    
        int sensor_init_status = thermal_sensor_init(
        &sensors[i],
        sensors[i].address);

     if (sensor_init_status == -1)
    {
        printf(
            "[THERMALS] Sensor #%u initialization failed: invalid address\n",
            (unsigned int)sensors[i].sensor_id
        );
        fflush(stdout);
        return;
    }
    else if (sensor_init_status == -2)
    {
        printf(
            "[THERMALS] Sensor #%u initialization failed: "
            "maximum sensors already initialized\n",
            (unsigned int)sensors[i].sensor_id
        );
        fflush(stdout);
        return;
    }
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

        for (uint8_t i = 0; i < MAX_SENSORS; i++) {

            read_status = thermal_sensor_read(&sensors[i], &currentTemp);
                if (read_status < 0){
                    thermals_invalidate_sensor(sensors[i].sensor_id);
                    printf(
                        "[THERMALS] ERROR OCCURRED WHILE READING SENSOR\n"
                        "FAULTY SENSOR ADDRESS = %u\n"
                        "FAULTY SENSOR ID = %u\n",
                        (unsigned int)sensors[i].address,
                        (unsigned int)sensors[i].sensor_id);
                    fflush(stdout);}
                else { //executes if sensor read is succsessful

                if (currentTemp < MIN_VALID_TEMPERATURE_C ||
                    currentTemp > MAX_VALID_TEMPERATURE_C){
                        thermals_invalidate_sensor(sensors[i].sensor_id);
                        printf(
                            "[THERMAL_SENSOR_READ] Invalid temperature reading: %.2f C\n",
                            currentTemp);
                        fflush(stdout);} 
                    else {
                    // Update the global thermal data structure with the new temperature reading
                    thermals_set_current(currentTemp, sensors[i].sensor_id);
                }

            }
        }
        xTaskDelayUntil(
        &lastWakeTime,
        pdMS_TO_TICKS(SENSOR_READ_TASK_PERIOD_MS)
    );
    }
}
