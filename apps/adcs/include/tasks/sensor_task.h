/* Samples local ADCS sensors and publishes timestamped packets at 100 Hz. */
#ifndef ADCS_TASKS_SENSOR_TASK_H
#define ADCS_TASKS_SENSOR_TASK_H

#include "FreeRTOS.h"
#include "task.h"
#include "communication/message.h"

extern TaskHandle_t xSensorHandle;

/* Initializes sensor drivers and creates the statically allocated task. */
void sensor_task_init(void);

/* Reads each driver, validates freshness/range, and publishes one sensor packet. */
void sensor_task(void *pvParameters);

/* Receives the newest sensor packet; old packets are overwritten by design. */
int sensor_task_receive(adcs_sensor_packet_t *packet, TickType_t wait_ticks);

#endif
