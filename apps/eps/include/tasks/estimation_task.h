/*
 * Estimation Task
 * Consumes sensor packets and derives the power state (instant/average power,
 * accumulated energy, battery efficiency) that telemetry and the manager use.
 */
#ifndef EPS_TASKS_ESTIMATION_TASK_H
#define EPS_TASKS_ESTIMATION_TASK_H

#include "FreeRTOS.h"
#include "task.h"

extern TaskHandle_t xEstimationHandle;

void estimation_task_init(void);

void estimation_task(void *pvParameters);

#endif
