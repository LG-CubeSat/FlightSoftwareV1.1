/*
Housekeeping Task
Maintains basic homeostasis
Stack usage, CPU usage, Temp, Task heartbeat
*/

#ifndef HOUSEKEEPING_TASK_H
#define HOUSEKEEPING_TASK_H

#include "FreeRTOS.h"
#include "task.h"

void housekeeping_task_init(void);

void housekeeping_task(void *pvParameters);

#endif