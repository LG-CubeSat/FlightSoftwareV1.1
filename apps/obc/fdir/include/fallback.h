#ifndef OBC_FDIR_FALLBACK_H
#define OBC_FDIR_FALLBACK_H

#include "obc_supervisor_protocol.h"
#include "obc_ipc.h"

typedef enum {
    UNRESPONSIVE = 1,
    OUT_OF_BOUNDS = 2
} fault_type_t;

typedef struct {
    OBC_Roles_t role;
    fault_type_t fault_type;
} fault_report_t;


int fallback_request(supervisor_cmd_t cmd, OBC_Roles_t role);
supervisor_request_t fallback_handle_fault(fault_report_t report);

#endif