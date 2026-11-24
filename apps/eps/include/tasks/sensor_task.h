/*
Sensor Task
fastest task of all (100-200hz)
*/

#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

#include "FreeRTOS.h"
#include "task.h"

#include <stdbool.h>

extern TaskHandle_t xSensorHandle;

void sensor_task_init(void);

void sensor_task(void *pvParameters);

//all these functions are for reading the actual hw, returns bool for success or fail and stores data in a float that is dereferenced

bool read_solar_voltage(float *solar_voltage);

bool read_solar_current(float *solar_current);

bool read_solar_shunt_voltage(float *solar_shunt_voltage);

bool read_battery_voltage(float *battery_voltage);

bool read_battery_current(float *battery_current);

bool read_battery_shunt_voltage(float *battery_shunt_voltage);

bool read_battery_temperature(float *battery_temperature);



#endif