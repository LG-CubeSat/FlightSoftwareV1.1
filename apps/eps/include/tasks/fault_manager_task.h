#ifndef FAULT_MANAGER_TASK_H
#define FAULT_MANAGER_TASK_H

#include "FreeRTOS.h"
#include "task.h"

void fault_manager_task_init(void);

void fault_manager_task(void *pvParameters);

#endif //FAULT_MANAGER_TASK_H