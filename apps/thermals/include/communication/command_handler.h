#ifndef THERMALS_COMMAND_HANDLER_H
#define THERMALS_COMMAND_HANDLER_H

#include <stdint.h>

#include "csp_commands.h"


//command IDs for set temp and request temp(s)
typedef enum
{
    THERMALS_WIRE_COMMAND_SET_TARGET_TEMP = 80,
    THERMALS_WIRE_COMMAND_REQUEST_TELEMETRY = 81
} thermals_wire_command_id_t;

void command_handler_init(void);

#endif