#include <stdio.h>

#include "command_task.h"

#include "command.h"
#include "command_queue.h"

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

    printf("    Destination: 0x%02X\n", command.destination);
    printf("    Sequence: %d\n", command.sequence);
    printf("    Command: 0x%2X\n", command.command);
}
