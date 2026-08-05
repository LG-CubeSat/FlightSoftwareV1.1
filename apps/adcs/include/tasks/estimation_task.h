/*
Estimation Task
Runs 50-100 HZ
waits for sensor packet
runs the kalman filter
publishes the current attitude
*/

#ifndef ESTIMATION_TASK_H
#define ESTIMATION_TASK_H

#include "FreeRTOS.h"
#include "task.h"

void estimation_task_init(void);

void estimation_task(void *pvParameters);

#endif