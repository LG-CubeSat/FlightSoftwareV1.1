/* Builds and transmits a coherent ADCS status snapshot at 5 Hz. */
#ifndef ADCS_TASKS_TELEMETRY_TASK_H
#define ADCS_TASKS_TELEMETRY_TASK_H

#include "FreeRTOS.h"
#include "task.h"

extern TaskHandle_t xTelemetryHandle;

/* Initializes telemetry state and creates the statically allocated task. */
void telemetry_task_init(void);

/* Snapshots manager state, explicitly encodes it, and sends it over CSP. */
void telemetry_task(void *pvParameters);

#endif
