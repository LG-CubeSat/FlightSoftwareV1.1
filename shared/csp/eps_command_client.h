#ifndef EPS_COMMAND_CLIENT_H
#define EPS_COMMAND_CLIENT_H

#include <stddef.h>
#include "csp_commands.h"

/* Acceptance only: an ACK does not confirm telemetry delivery or storage. */
typedef enum {
    EPS_COMMAND_ACCEPTED = 0,
    EPS_COMMAND_REJECTED = 1,
    EPS_COMMAND_INVALID_REQUEST = -1,
    EPS_COMMAND_CONNECT_FAILED = -2,
    EPS_COMMAND_NO_BUFFER = -3,
    EPS_COMMAND_TIMEOUT = -4,
    EPS_COMMAND_INVALID_REPLY = -5
} eps_command_result_t;

eps_command_result_t eps_command_send(const void *payload, size_t length);
const char *eps_command_result_name(eps_command_result_t result);

#endif
