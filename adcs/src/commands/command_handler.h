#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include "commands.h"

void command_handler_initialize(void);

void command_handler_process(
    const CommandMessage *command
);

#endif