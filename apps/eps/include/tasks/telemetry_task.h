/*
Telemetry Task
1-10HZ
Collects everything
Sends out data using CSP
*/

#ifndef TELEMETRY_TASK_H
#define TELEMETRY_TASK_H

#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"

extern TaskHandle_t xTelemetryHandle;

void telemetry_task_init(void);

void telemetry_task(void *pvParamaters);

/* Thread-safe, coalescing request. True means queued, not delivered. */
bool telemetry_request_send(void);

#endif
