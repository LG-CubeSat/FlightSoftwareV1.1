/*
Telemetry Task
1-10HZ
Collects everything
Sends out data using CSP
*/

#ifndef TELEMETRY_TASK_H
#define TELEMETRY_TASK_H

#include "FreeRTOS.h"
#include "task.h"

void telemetry_task_init(void);

void telemetry_task(void *pvParamaters);

#endif