#include "obc_ipc.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <errno.h>

#include <pthread.h>
#include "frame.h"

#define IPC_BACKLOG 5
#define MAX_IPC_PAYLOAD 256

static OBC_Roles_t my_role;
static int bus_fd = -1;

// grabs socket path for role
static const char *path_for_role(OBC_Roles_t role)
{
    switch (role) {
        case ROLE_COMMANDS: return "/tmp/obc_ipc_commands.sock";
        case ROLE_COMPUTE: return "/tmp/obc_ipc_compute.sock";
        case ROLE_DATA: return "/tmp/obc_ipc_data.sock";
        case ROLE_FDIR: return "/tmp/obc_ipc_fdir.sock";
        case ROLE_MISSION: return "/tmp/obc_ipc_mission.sock";
        case ROLE_SUPERVISOR: return "/tmp/obc_ipc_supervisor.sock";
        case ROLE_TIME: return "/tmp/obc_ipc_time.sock";
        defualt: return NULL;
    }
}

typedef struct {
    uint8_t dest;
    uint8_t src;
    uint16_t length;
    uint8_t payload[MAX_IPC_PAYLOAD];
} IPCFrame;
