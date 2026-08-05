/*
Command Task
Continously waits for ground messages or messages from other boards
Decodes them and updates manager
*/

#ifndef COMMAND_TASK_H
#define CONTROL_TASK_H

#include "FreeRTOS.h"
#include "task.h"

void command_task_init(void);

void command_task(void *pvParameters);

#endif