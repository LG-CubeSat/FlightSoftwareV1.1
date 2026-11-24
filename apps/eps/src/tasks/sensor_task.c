/* Collect one EPS sensor snapshot every 100 ms (10 Hz). */

#include "tasks/sensor_task.h"

#include "FreeRTOS.h"
#include "task.h"

#include "eps_data.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "powerCalculations.h"
#include "conversions.h"

#define SENSOR_TASK_PRIORITY (3)
#define SENSOR_TASK_STACK_SIZE (1024)
#define SENSOR_TASK_PERIOD_MS (100)

static uint32_t sensor_sequence = 0U;

static StackType_t xSensorTaskStack[SENSOR_TASK_STACK_SIZE];
static StaticTask_t xSensorTaskBuffer;

TaskHandle_t xSensorHandle = NULL;


void sensor_task_init(void)
{
    xSensorHandle = xTaskCreateStatic(
        sensor_task,
        "sensor",
        SENSOR_TASK_STACK_SIZE,
        NULL,
        SENSOR_TASK_PRIORITY,
        xSensorTaskStack,
        &xSensorTaskBuffer
    );


    if (xSensorHandle == NULL)
    {
        printf("[SENSOR] Failed to initialize\n");
    }
}

void sensor_task(void *pvParameters)
{
    (void) pvParameters;

    TickType_t lastWakeTime = xTaskGetTickCount();

    eps_sensor_data_t local_sensor_data;

    for (;;)
    {
        
        //read data
        local_sensor_data = get_eps_sensor_data();

        //set all validity bits to 0, this way if a reading was once successful but is now not, it needs to repetitevly make sure data is valid each iteration
        local_sensor_data.valid_sensor_mask = 0;


        //time shit
        TickType_t now_ticks = xTaskGetTickCount();
        local_sensor_data.timestamp_ms = (uint32_t)pdTICKS_TO_MS(now_ticks);
        local_sensor_data.sequence = ++sensor_sequence;

        //set data
        set_eps_sensor_data(local_sensor_data);

    

        xTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(SENSOR_TASK_PERIOD_MS)
        );
    }
}

    /*
    example implentation
    
    if (read data from hw driver is successful) {
    
    update data;
    return true;
    } else {
     
    report to OBC or smth
    return false; *data will be updated to NaN in the loop, doesn't need to be here
    }
     
    */
bool read_solar_voltage(float *solar_voltage) {

    //placeholder reading, to be implemented w/ hw
    *solar_voltage = 7.4f;
    return true;

}

bool read_solar_current(float *solar_current){

    //placeholder reading, to be implemented w/ hw
    *solar_current = .3f;
    return true;

}

bool read_solar_shunt_voltage(float *solar_shunt_voltage){

    //placeholder reading, to be implemented w/ hw
    *solar_shunt_voltage = millivolts_to_volts(10);
    return true;

}

bool read_battery_voltage(float *battery_voltage){

    //placeholder reading, to be implemented w/ hw
    *battery_voltage = 10.0f;
    return true;

}

bool read_battery_current(float *battery_current){

    //placeholder reading, to be implemented w/ hw
    *battery_current = 1.5f;
    return true;

}

bool read_battery_shunt_voltage(float *battery_shunt_voltage){

    //placeholder reading, to be implemented w/ hw
    *battery_shunt_voltage = millivolts_to_volts(20);
    return true;

}

bool read_battery_temperature(float *battery_temperature) {

    //placeholder reading, to be implemented w/ hw
    *battery_temperature = fahrenheit_to_celsius(80);
    return true;

}
