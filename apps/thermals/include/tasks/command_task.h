#ifndef THERMALS_COMMAND_TASK_H
#define THERMALS_COMMAND_TASK_H

#include "FreeRTOS.h"
#include "task.h"

#include "communication/message.h"

void command_task_init(void);

void command_task(void *pvParameters);

int command_task_send(const thermals_command_t *message);

#endif // THERMALS_COMMAND_TASK_H
