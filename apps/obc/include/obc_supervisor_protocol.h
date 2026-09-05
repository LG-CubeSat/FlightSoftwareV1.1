#ifndef OBC_SUPERVISOR_PROTOCOL_H
#define OBC_SUPERVISOR_PROTOCOL_H

#include <stdint.h>

/* Wire format for FDIR's requests to supervisor over ipc.
   Both sides must agree on this exact layout -- supervisor is the
   receiver (see shutdown_thread in apps/obc/supervisor), FDIR is the
   sender (see fallback.c in apps/obc/fdir). */
typedef enum {
    SUPERVISOR_CMD_SHUTDOWN = 1,
    SUPERVISOR_CMD_RESTART = 2,
} supervisor_cmd_t;

typedef struct {
    uint8_t cmd;   // supervisor_cmd_t
    uint8_t role;  // OBC_Roles_t
} supervisor_request_t;

#endif // OBC_SUPERVISOR_PROTOCOL_H
