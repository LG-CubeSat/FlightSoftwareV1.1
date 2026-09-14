/* Monitors task health, temperature, stack margins, and watchdog liveness. */
#ifndef ADCS_TASKS_HOUSEKEEPING_TASK_H
#define ADCS_TASKS_HOUSEKEEPING_TASK_H

#include "FreeRTOS.h"
#include "task.h"

extern TaskHandle_t xHousekeepingHandle;

/* Creates the statically allocated low-priority health-monitor task. */
void housekeeping_task_init(void);

/* Evaluates health once per second, reports faults, and pets the watchdog. */
void housekeeping_task(void *pvParameters);

#endif
