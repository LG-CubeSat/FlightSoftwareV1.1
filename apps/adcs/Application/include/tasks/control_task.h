/* Runs the deterministic magnetorquer control pipeline at 20 Hz. */
#ifndef ADCS_TASKS_CONTROL_TASK_H
#define ADCS_TASKS_CONTROL_TASK_H

#include "FreeRTOS.h"
#include "task.h"

extern TaskHandle_t xControlHandle;

/* Initializes control modules and creates the statically allocated task. */
void control_task_init(void);

/* Consumes the manager snapshot, selects a controller, and commands SIM actuators. */
void control_task(void *pvParameters);

/* Manual override for telecommands to toggle magnetorquer channels directly. */
void control_task_set_manual_magnetorquer(uint8_t axis, int8_t direction);

#endif
