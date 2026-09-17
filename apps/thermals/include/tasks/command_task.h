#ifndef COMMAND_TASK_H
#define COMMAND_TASK_H

#include "FreeRTOS.h"
#include "task.h"

/*
Command table

1 - @param parameter Recieved from obc, sets goal temperature to @param paramater
2 - Obc requests current temp and goal temp, send via telem

*/

#define THERMAL_CMD_SET_GOAL_TEMP 1
#define THERMAL_CMD_REQUEST_TELEMETRY 2


typedef struct
{
    uint32_t command; // THERMAL_CMD_SET_GOAL_TEMP or THERMAL_CMD_REQUEST_TELEMETRY
    float parameter; // Signed float32 temperature in degrees Celsius

} CommandMessage_t;

void command_task_init(void);

void command_task(void *pvParameters);

int command_task_send(const CommandMessage_t *message);

#endif // COMMAND_TASK_H