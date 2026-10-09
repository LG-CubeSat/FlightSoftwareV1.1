/*
 * See: estimation_task.c
*/

#ifndef ESTIMATION_TASK_H
#define ESTIMATION_TASK_H

#include "FreeRTOS.h"
#include "task.h"

extern TaskHandle_t xEstimationHandle;

void estimation_task_init(void);

void estimation_task(void *pvParameters);

#endif
