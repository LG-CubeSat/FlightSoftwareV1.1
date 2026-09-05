#include "fallback.h"

#include "obc_supervisor_protocol.h"
#include "obc_ipc.h"

#include <stdio.h>
#include <string.h>

/* Actually determines how the issues will map into a new response */

int fallback_request(supervisor_cmd_t cmd, OBC_Roles_t role)
{
    supervisor_request_t req = { .cmd = (uint8_t)cmd, .role = (uint8_t)role };
    int rc = IPC_send(ROLE_SUPERVISOR, (const uint8_t *)&req, sizeof(req));
    if (rc < 0) {
        fprintf(stderr, "[FDIR FALLBACK] failed to request send (cmd=%d, role=%d)\n", cmd, role);
        return -1;
    }
    return 0;
}

int fallback_handle_fault(fault_report_t report)
{
    supervisor_cmd_t cmd;
    // logic based on report, TODO: add logic
    switch (report.fault_type) {
        case UNRESPONSIVE: cmd = SUPERVISOR_CMD_RESTART; break;
        case OUT_OF_BOUNDS: cmd = SUPERVISOR_CMD_RESTART; break;
        default: return 1;
    }

    if (fallback_request(cmd, report.role) != 0) {
        printf("[FDIR FALLBACK] Fallback request failed\n");
        return 2;
    }
    return 0;
}

/* Mechanism only, same shape as fallback_request: builds the wire
   message and sends it. Deciding *when* a board deserves a reset is
   future policy work -- there's no external-board fault detection yet. */
int fallback_reset_board(uint8_t board_addr, uint8_t board_port)
{
    command_envelope_t reset_cmd = { .command_id = CMD_RESET, .seq = 1 };
    relay_request_t req = { .dest_addr = board_addr, .dest_port = board_port, .length = sizeof(reset_cmd) };
    memcpy(req.payload, &reset_cmd, sizeof(reset_cmd));

    int rc = IPC_send(ROLE_COMMANDS, (const uint8_t *)&req, sizeof(req));
    if (rc < 0) {
        fprintf(stderr, "[FDIR FALLBACK] failed to send board reset (addr=%d, port=%d)\n", board_addr, board_port);
        return -1;
    }
    return 0;
}

int fallback_shutdown_board(uint8_t board_addr, uint8_t board_port)
{
    command_envelope_t shutdown_cmd = { .command_id = CMD_SHUTDOWN, .seq = 1 };
    relay_request_t req = { .dest_addr = board_addr, .dest_port = board_port, .length = sizeof(shutdown_cmd) };
    memcpy(req.payload, &shutdown_cmd, sizeof(shutdown_cmd));

    int rc = IPC_send(ROLE_COMMANDS, (const uint8_t *)&req, sizeof(req));
    if (rc < 0) {
        fprintf(stderr, "[FDIR FALLBACK] failed to send board shutdown (addr=%d, port=%d)\n", board_addr, board_port);
        return -1;
    }
    return 0;
}