/* Receives, validates, acknowledges, and queues commands addressed to EPS. */
#ifndef EPS_COMMUNICATION_COMMAND_HANDLER_H
#define EPS_COMMUNICATION_COMMAND_HANDLER_H

#include <stdint.h>

#include "communication/message.h"
#include "csp_commands.h"

/*
 * Starts the CSP listener for EPS_CMD_PORT. The listener performs only wire
 * decoding and command acknowledgement; power/mode work belongs to the
 * command task and manager.
 */
void command_handler_init(void);

#endif
