/* Blocks on decoded commands and hands validated requests to the ADCS manager. */
#ifndef ADCS_TASKS_COMMAND_TASK_H
#define ADCS_TASKS_COMMAND_TASK_H

#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "communication/message.h"

/* Creates the static command queue and FreeRTOS task. */
void command_task_init(void);

/* Receives commands indefinitely; command work is delegated to domain modules. */
void command_task(void *pvParameters);

/* Enqueues one decoded command from the CSP receive context. */
int command_task_send(const adcs_command_t *message);

#endif
