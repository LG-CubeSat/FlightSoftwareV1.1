#include <stdio.h>

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

void command_task_initialize(void)
{
    printf("[COMMAND TASK] Initialized.\n");
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
