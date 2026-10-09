/*
 * Housekeeping Task
 * Maintains basic homeostasis: fault evaluation, health counters, stack
 * margin, watchdog petting, and the periodic manager update (1Hz).
 */
#ifndef EPS_TASKS_HOUSEKEEPING_TASK_H
#define EPS_TASKS_HOUSEKEEPING_TASK_H

#include "FreeRTOS.h"
#include "task.h"

extern TaskHandle_t xHousekeepingHandle;

void housekeeping_task_init(void);

void housekeeping_task(void *pvParameters);

#endif
