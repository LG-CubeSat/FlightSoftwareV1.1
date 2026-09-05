#ifndef OBC_FDIR_FALLBACK_H
#define OBC_FDIR_FALLBACK_H

#include "obc_supervisor_protocol.h"
#include "obc_relay_protocol.h"
#include "obc_ipc.h"
#include "csp_commands.h"

typedef enum {
    UNRESPONSIVE = 1,
    OUT_OF_BOUNDS = 2
} fault_type_t;

typedef struct {
    OBC_Roles_t role;
    fault_type_t fault_type;
} fault_report_t;


int fallback_request(supervisor_cmd_t cmd, OBC_Roles_t role);
int fallback_handle_fault(fault_report_t report);

/* Asks `commands` to relay a CMD_RESET to an external board (not a
   local OBC role -- use fallback_request/SUPERVISOR_CMD_RESTART for those). */
int fallback_reset_board(uint8_t board_addr, uint8_t board_port);

#endif