#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdint.h>
// create a shared command id for all components to accept and uphold
typedef enum
{
    CMD_POINT_TO_SUN = 0x01,
    CMD_POINT_TO_EARTH = 0x02,
    CMD_ENTER_SAFE_MODE = 0x03,
    CMD_EXIT_SAFE_MODE = 0x04,
    CMD_GET_ATTITUDE = 0x05,
    CMD_GET_STATUS = 0x06
} CommandId;

#endif