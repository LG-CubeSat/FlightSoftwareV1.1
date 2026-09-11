#ifndef COMMAND_TASK_H
#define COMMAND_TASK_H

#include "FreeRTOS.h"
#include "task.h"

typedef struct
{
    uint32_t command;
    float parameter;
} CommandMessage_t;

void command_task_init(void);

void command_task(void *pvParameters);

int command_task_send(const CommandMessage_t *message);


#endif // COMMAND_TASK_H