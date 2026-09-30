#ifndef THERMALS_COMMUNICATION_MESSAGE_H
#define THERMALS_COMMUNICATION_MESSAGE_H

#include <stdint.h>

typedef enum {

    THERMALS_COMMAND_NONE = 0,
    THERMALS_COMMAND_SET_TARGET_TEMP,
    THERMALS_COMMAND_REQUEST_TELEMETRY

} thermals_command_type_t;

typedef struct {

    uint32_t sequence;
    thermals_command_type_t type;

    union {
        float target_temp;
    } parameter;


} thermals_command_t;


#endif //THERMALS_COMMUNCATION_MESSAGE_H