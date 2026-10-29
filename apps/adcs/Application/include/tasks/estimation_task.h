/* Runs reference-vector generation and attitude estimation at sensor cadence. */
#ifndef ADCS_TASKS_ESTIMATION_TASK_H
#define ADCS_TASKS_ESTIMATION_TASK_H

#include "FreeRTOS.h"
#include "task.h"

extern TaskHandle_t xEstimationHandle;

/* Initializes estimator state and creates the statically allocated task. */
void estimation_task_init(void);

/* Consumes sensor packets and publishes the newest coherent attitude estimate. */
void estimation_task(void *pvParameters);

/* Requests a clean estimator re-seed on the estimation task's next cycle. */
void estimation_task_request_reset(void);

#endif
