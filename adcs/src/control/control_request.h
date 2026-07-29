#ifndef CONTROL_REQUEST_H
#define CONTROL_REQUEST_H

#include "adcs_context.h"

/*
NOTES:
This is how the Command Handler can translate the command sent to it
It will send a message in the form of:
ControlRequestType, Target
*/

typedef enum
{
    CONTROL_REQUEST_NONE = 0,

    CONTROL_REQUEST_ENTER_SAFE,

    CONTROL_REQUEST_EXIT_SAFE,

    CONTROL_REQUEST_POINT
} ControlReuqestType;

typedef struct
{
    ControlReuqestType type;

    AdcsTarget target;

} ControlRequest;

#endif