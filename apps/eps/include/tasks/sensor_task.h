/*
 * Sensor Task
 * Fastest task of all (10Hz).
 * Reads battery, solar array, and power-rail state, publishes one sensor
 * packet to the manager and the estimation task.
 */
#ifndef EPS_TASKS_SENSOR_TASK_H
#define EPS_TASKS_SENSOR_TASK_H

#include "FreeRTOS.h"
#include "task.h"

#include "communication/message.h"

extern TaskHandle_t xSensorHandle;

void sensor_task_init(void);

void sensor_task(void *pvParameters);

/* Blocks for the next published sensor packet, for the estimation task. */
int sensor_task_receive(eps_sensor_packet_t *packet, TickType_t wait_ticks);

#endif
