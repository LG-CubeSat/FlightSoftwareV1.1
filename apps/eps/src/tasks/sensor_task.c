/* Collect one EPS sensor snapshot every 100 ms (10 Hz). */

#include "tasks/sensor_task.h"

#include "FreeRTOS.h"
#include "task.h"

#include "eps_data.h"
#include "sensors/eps_sensor_reader.h"

#include <math.h>
#include <stdio.h>

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

    for (;;)
    {
        eps_sensor_data_t sample = {0};
        float reading;

        /* A field becomes valid only after a successful, finite reading. */
        if (eps_read_solar_voltage_v(&reading) && isfinite(reading) && reading >= 0.0f) {
            sample.solar_voltage_reading = reading;
            sample.valid_sensor_mask |= EPS_SOLAR_VOLTAGE;
        }
        if (eps_read_solar_current_a(&reading) && isfinite(reading)) {
            sample.solar_current_reading = reading;
            sample.valid_sensor_mask |= EPS_SOLAR_CURRENT;
        }
        if (eps_read_solar_shunt_voltage_mv(&reading) && isfinite(reading)) {
            sample.solar_shunt_voltage_reading = reading;
            sample.valid_sensor_mask |= SOLAR_SHUNT_VOLTAGE;
        }
        if (eps_read_battery_voltage_v(&reading) && isfinite(reading) && reading >= 0.0f) {
            sample.battery_voltage_reading = reading;
            sample.valid_sensor_mask |= EPS_BATTERY_VOLTAGE;
        }
        if (eps_read_battery_current_a(&reading) && isfinite(reading)) {
            sample.battery_current_reading = reading;
            sample.valid_sensor_mask |= EPS_BATTERY_CURRENT;
        }
        if (eps_read_battery_shunt_voltage_mv(&reading) && isfinite(reading)) {
            sample.battery_shunt_voltage_reading = reading;
            sample.valid_sensor_mask |= BATTERY_SHUNT_VOLTAGE;
        }
        if (eps_read_battery_temperature_c(&reading) && isfinite(reading)) {
            sample.battery_temperature_reading = reading;
            sample.valid_sensor_mask |= EPS_BATTERY_TEMPERATURE;
        }

        /* Publish this acquisition attempt even if every read failed. */
        sample.timestamp_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        sample.sequence = ++sensor_sequence;
        set_eps_sensor_data(sample);

        xTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(SENSOR_TASK_PERIOD_MS)
        );
    }
}
