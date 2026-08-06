/*
Command Task
Continously waits for ground messages or messages from other boards
Decodes them and updates manager
*/

#ifndef COMMAND_TASK_H
#define CONTROL_TASK_H

#include "FreeRTOS.h"
#include "task.h"
#include "../../../../libs/libcsp/include/csp/csp_types.h"

// TODO: expand
typedef struct
{
    uint32_t command;
    uint32_t parameter;
} CommandMessage_t;

void command_task_init(void);

void command_task(void *pvParameters);

int command_task_send(csp_packet_t *command);

#endif