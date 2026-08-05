/*
Control Task
runs 20-50 hz
Waits for attitude
Asks Manager
Runs correct controller, commands magnetorquers
*/

#ifndef CONTROL_TASK_H
#define CONTROL_TASK_H

#include "FreeRTOS.h"
#include "task.h"

void control_task_init(void);

void control_task(void *pvParameters);

#endif