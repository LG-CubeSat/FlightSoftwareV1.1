/*
Sensor Task
fastest task of all (100-200hz)
Loops
Read imu, read magnetometers, read sun sensors, read camera status, publish sensor packet
*/

#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

#include "FreeRTOS.h"
#include "task.h"

extern TaskHandle_t xSensorHandle;

void sensor_task_init(void);

void sensor_task(void *pvParameters);

#endif