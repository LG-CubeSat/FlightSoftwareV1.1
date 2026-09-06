#ifndef THERMAL_WRITE_TASK_H
#define THERMAL_WRITE_TASK_H

#include "FreeRTOS.h"
#include "task.h"

extern TaskHandle_t xThermalWriteHandle;

void thermal_write_task_init(void);

void thermal_write_task(void *pvParameters);

#endif
