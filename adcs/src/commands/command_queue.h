#ifndef COMMAND_QUEUE_H
#define COMMAND_QUEUE_H

#include "commands.h"

void command_queue_initialize(void);

int command_queue_push(
    CommandMessage command
);

int command_queue_pop(
    CommandMessage *command
);

#endif