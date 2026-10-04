#ifndef OBC_IPC_H
#define OBC_IPC_H

#include <stdint.h>

#define OBC_IPC_MAX_PAYLOAD 256U

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
    ROLE_CAM = 8, // sorta not really a role
    ROLE_COUNT // not a role: array sizing bound, keep last
} OBC_Roles_t;

// inline has compiler replace all code references to this function with code. Rather than use JUMP. Can be quicker, if function is small.
static inline int role_is_valid(unsigned v) {
    return v>=ROLE_COMMANDS && v < ROLE_COUNT;
}

// init
IPC_Status_t IPC_initialize(OBC_Roles_t role);
int IPC_send(OBC_Roles_t role_dest, const uint8_t *data, uint16_t length);
int IPC_receive(OBC_Roles_t *src_role, uint8_t *buffer, uint16_t max_length);

/* 
Like IPC receive but gives up after timout. Returns the IPC_TIMEOUT.
timeout_ms < 0 waits forever (what IPC_receive itself passes).
*/
int IPC_receive_timeout(OBC_Roles_t *src_role, uint8_t *buffer, uint16_t max_length, int timeout_ms);

#endif // OBC_IPC_H