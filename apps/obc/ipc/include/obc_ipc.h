#ifndef OBC_IPC_H
#define OBC_IPC_H

#include <stdint.h>

typedef enum {
    IPC_OK = 0,
    IPC_ERROR = -1,
    IPC_TIMEOUT = -2
} IPC_Status_t;

// roles
typedef enum {
    ROLE_COMMANDS = 1,
    ROLE_COMPUTE = 2,
    ROLE_DATA = 3,
    ROLE_FDIR = 4,
    ROLE_MISSION = 5,
    ROLE_SUPERVISOR = 6,
    ROLE_TIME = 7,
    
} OBC_Roles_t;

// init
IPC_Status_t IPC_initialize(OBC_roles_t role);
int IPC_send(OBC_Roles_t role_dest, const uint8_t *data, uint16_t length);
int IPC_receive(OBC_Roles_t *src_role, uint8_t *buffer, uint16_t max_length);

#endif // OBC_IPC_H