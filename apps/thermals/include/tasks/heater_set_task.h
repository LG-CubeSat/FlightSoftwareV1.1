#ifndef HEATER_SET_TASK_H
#define HEATER_SET_TASK_H

#include "FreeRTOS.h"
#include "task.h"

extern TaskHandle_t xHeaterSetTask;

void heater_set_task_init(void);

void heater_set_task(void *pvParameters);

#endif //HEATER_SET_TASK_H
