#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "command_task.h"

#include "commands.h"
#include "command_queue.h"
#include "command_handler.h"

/*
Notes:
- Uses the command queue and fetches a command
- Sends command to the handler to be interpreted
- Doesn't do anything with the command simply grabs it and sends it.
*/

static StaticTask_t xCommandTaskBuffer;
static StackType_t xCommandStack[configMINIMAL_STACK_SIZE];

void command_task_initialize(void)
{
    printf("[COMMAND TASK] Initialized.\n");
}

void command_task_loop(void *pvParameters)
{
    printf("[COMMAND TASK] Task Started (Blocking).\n");

    for (;;)
    {
        command_task_run();
        // Since command_queue_pop is non-blocking right now,
        // we add a small delay to prevent 100% CPU usage.
        // In a real system, the queue pop would block.
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}

void command_task_run(void)
{
    // make empty command
    CommandMessage command;

    // command gets written to our empty command
    if (!command_queue_pop(&command)) {
        // if command queue is empty then return
        return;
    }

    printf("[COMMAND TASK] Processing command.\n");

    command_handler_process(&command);
}

TaskHandle_t command_task_create_static(void) {
    return xTaskCreateStatic(
        command_task_loop,
        "Command Task",
        configMINIMAL_STACK_SIZE,
        NULL,
        1,
        xCommandStack,
        &xCommandTaskBuffer
    );
}