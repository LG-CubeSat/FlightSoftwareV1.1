#ifndef THERMALS_COMMAND_TASK_H
#define THERMALS_COMMAND_TASK_H

#include "FreeRTOS.h"
#include "task.h"

/*
Command table:

1 - Receives a target temperature from the OBC.
2 - Receives an OBC request for thermal telemetry.

*/


#define THERMAL_CMD_SET_TARGET_TEMP 1
#define THERMAL_CMD_REQUEST_TELEMETRY 2


typedef struct
{
    uint32_t command; // THERMAL_CMD_SET_TARGET_TEMP or THERMAL_CMD_REQUEST_TELEMETRY
    float parameter;  // Signed float32 temperature in degrees Celsius
} CommandMessage_t;

void command_task_init(void);

void command_task(void *pvParameters);

int command_task_send(const CommandMessage_t *message);

#endif // THERMALS_COMMAND_TASK_H
