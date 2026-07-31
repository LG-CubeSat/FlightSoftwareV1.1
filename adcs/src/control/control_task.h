#ifndef CONTROL_TASK_H
#define CONTROL_TASK_H

#include "FreeRTOS.h"
#include "task.h"

void control_task_initialize(void);

void control_task_run(void);

void control_task_loop(void *pvParameters);

TaskHandle_t control_task_create_static(void);

#endif