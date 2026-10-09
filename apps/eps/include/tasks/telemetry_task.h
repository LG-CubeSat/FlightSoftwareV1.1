/*
 * Telemetry Task
 * Collects the manager snapshot and sends it to the OBC over CSP (1Hz).
 */
#ifndef EPS_TASKS_TELEMETRY_TASK_H
#define EPS_TASKS_TELEMETRY_TASK_H

#include "FreeRTOS.h"
#include "task.h"

extern TaskHandle_t xTelemetryHandle;

void telemetry_task_init(void);

void telemetry_task(void *pvParameters);

#endif
