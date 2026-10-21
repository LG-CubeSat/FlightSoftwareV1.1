#ifndef THERMALS_THERMAL_MODEL_TASK_H
#define THERMALS_THERMAL_MODEL_TASK_H

#include "FreeRTOS.h"
#include "task.h"

extern TaskHandle_t xThermalModelHandle;

void thermal_model_task_init(void);

void thermal_model_task(void *pvParameters);

#endif // THERMALS_THERMAL_MODEL_TASK_H