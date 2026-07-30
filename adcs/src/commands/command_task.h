#ifndef COMMAND_TASK_H
#define COMMAND_TASK_H

#include "FreeRTOS.h"
#include "task.h"

void command_task_initialize(void);

void command_task_run(void);

void command_task_loop(void *pvParameters);

TaskHandle_t command_task_create_static(void);

#endif