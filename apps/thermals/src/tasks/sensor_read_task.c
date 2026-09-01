#include "../../include/thermal_sensor.h"

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"

#include "stdint.h"


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
    thermal_sensor_t sensor1;
    sensor1.ADDRESS = 0x48;
    sensor1.SENSOR_GENERIC = 1;
    
    thermal_sensor_t sensor2;
    sensor2.ADDRESS = 0x40;
    sensor2.SENSOR_GENERIC = 2;

    int sensor_1_init_value = thermal_sensor_init(&sensor1, sensor1.ADDRESS);
    if (sensor_1_init_value == -1) {
        printf("INVALID ADDRESS\n");
    } else if (sensor_1_init_value == -2) {
        printf("2 SENSORS ALREADY INITIALIZED\n");
    } else if (sensor_1_init_value == 1) {
        printf("SENSOR1 INITIALIZED SUCCSESSFULY");
    }

    int sensor_2_init_value = thermal_sensor_init(&sensor2, sensor2.ADDRESS);
    if (sensor_2_init_value == -1) {
        printf("INVALID ADDRESS\n");
    } else if (sensor_2_init_value == -2) {
        printf("2 SENSORS ALREADY INITIALIZED\n");
    } else if (sensor_2_init_value == 1) {
        printf("SENSOR2 INITIALIZED SUCCSESSFULY");
    }
 
    for (;;) {

        //reads data from sensors 1 & 2 and writes data to global temp variables stored in main.c
        temperature1 = read_temperature_sensor(&sensor1);
        temperature2 = read_temperature_sensor(&sensor2);

        xTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(SENSOR_READ_TASK_PERIOD_MS)
        );
    }

}
