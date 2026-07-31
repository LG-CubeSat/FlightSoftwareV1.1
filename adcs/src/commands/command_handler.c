#include <stdio.h>

#include "command_handler.h"
#include "../../../shared/protocol/commands.h"
#include "../control/control_request.h"
#include "../control/control_queue.h"

/* 
Notes: 
Uses shared commands
Does not directly command the ADCS
Rather it requests a change in state and target to the control queue.
IT INTERPRETS THE COMMAND.
*/

void command_handler_initialize()
{
    printf("[COMMAND HANDLER] Initialzied\n.");
}

void command_handler_process(const CommandMessage *command)
{
    ControlRequest request;
    
    switch (command->command)
    {
        case CMD_POINT_TO_SUN:
            printf(
                "[COMMAND HANDLER] Request: Point to Sun.\n"
            );

            adcs_context_set_target_angle(45.0f);
            request.type = CONTROL_REQUEST_POINT;
            request.target = ADCS_TARGET_SUN;

            control_queue_push(request);

            break;

        case CMD_POINT_TO_EARTH:
            printf("[COMMAND HANDLER] Request: Point to Earth.\n");

            request.type = CONTROL_REQUEST_POINT;
            request.target = ADCS_TARGET_EARTH;

            control_queue_push(request);

            break;

        case CMD_ENTER_SAFE_MODE:
            printf("[COMMAND HANDLER] Request: Enter Safe Mode.\n");
            
            request.type = CONTROL_REQUEST_ENTER_SAFE;
            request.target = ADCS_TARGET_NONE;

            control_queue_push(request);

            break;

        case CMD_EXIT_SAFE_MODE:
            printf("[COMMAND HANDLER] Request: Exit Safe Mode.\n");

            request.type = CONTROL_REQUEST_EXIT_SAFE;
            request.target = ADCS_TARGET_NONE;

            control_queue_push(request);

            break;

        case CMD_GET_ATTITUDE:
            printf("[COMMAND HANDLER] Request: Getting Altitude.\n");
            break;
        case CMD_GET_STATUS:
            printf("[COMMAND HANDLER] Request: Getting Status.\n");
            break;
        
        default:
            printf("[COMMAND HANDLER] ERROR: Unknown Command.\n");
    }
}