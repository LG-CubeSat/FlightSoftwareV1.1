#ifndef SENSOR_READ_TASK_H
#define SENSOR_READ_TASK_H

#include "FreeRTOS.h"
#include "task.h"

extern TaskHandle_t xSensorReadHandle;

void sensor_read_task_init(void);

void sensor_read_task(void *pvParameters);

#endif
