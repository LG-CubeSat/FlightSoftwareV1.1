#include "fallback.h"

#include "obc_supervisor_protocol.h"
#include "obc_ipc.h"

#include <stdio.h>

/* Actually determines how the issues will map into a new response */

/* Reactive thread that responds to fallback calls. This is what queries the supervisor */
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
    // logic based on report, TODO: add logic
    if (report.fault_type != OUT_OF_BOUNDS && report.fault_type != UNRESPONSIVE)
    {
        printf("[FDIR FALLBACK] Fault type not valid.\n");
        return 1;
    }
    
    // if (report.role) {} <-- fill logic here
    supervisor_cmd_t cmd = SUPERVISOR_CMD_SHUTDOWN;
    if (fallback_request(cmd, report.role) != 0) {
        printf("[FDIR FALLBACK] Fallback request failed\n");
        return 2;
    }
    return 0;
}