#include <stdio.h>

#include "command_handler.h"
#include "../../../shared/protocol/commands.h"

/* 
Notes: 
Uses shared commands
Does not directly command the ADCS
Rather it sets a state.
IT INTERPRETS THE COMMAND
*/

void command_handler_initialize()
{
    printf("[COMMAND HANDLER] Initialzied\n.");
}

void command_handler_process(const CommandMessage *command)
{
    printf("[COMMAND HANDLER] Received command 0x%2X.\n", command->command);

    switch (command->command)
    {
        case CMD_POINT_TO_SUN:
            printf("[COMMAND HANDLER] Request: Point to Sun");
            break;
        case CMD_POINT_TO_EARTH:
            printf("[COMMAND HANDLER] Request: Point to Earth.");
            break;
        case CMD_ENTER_SAFE_MODE:
            printf("[COMMAND HANDLER] Request: Entering Safe Mode");
            break;
        case CMD_EXIT_SAFE_MODE:
            printf("[COMMAND HANDLER] Request: Exiting Safe Mode");
            break;
        case CMD_GET_ATTITUDE:
            printf("[COMMAND HANDLER] Request: Getting Altitude");
            break;
        case CMD_GET_STATUS:
            printf("[COMMAND HANDLER] Request: Getting Status.");
            break;
        default:
            printf("[COMMAND HANDLER] ERROR: Unknown Command.\n");
    }
}