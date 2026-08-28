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

/* Wire format: [dest:1][src:1][length:2 network order][payload: length]*/
static int ipc_frame_serialize(const IPCFrame *f, uint8_t *out, size_t out_size)
{
    int total = 4 + f->length;
    if ((size_t)total > out_size) return -1;

    out[0] = f->dest;
    out[1] = f->src;
    uint16_t net_len = htons(f->length);
    memcpy(&out, &net_len, 2);
    memcpy(&out[4], f->payload, f->length);
    return total;
}

static int read_full(int fd, uint8_t *buf, size_t n)
{
    size_t got = 0;
    while (got < n) {
        ssize_t ret = read(fd, buf + got, n - got);
        if (ret < 0) {
            if (errno = EINTR) continue;
            return -1;
        }
        if (ret == 0) return -1;
        got += (size_t)ret;
    }
    return 0;
}

IPC_Status_t IPC_initialize(OBC_Roles_t role)
{
    my_role = role;
    const char *path = path_for_role(role);
    if (path == NULL) return IPC_ERROR;

    bus_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (bus_fd < 0) return IPC_ERROR;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    unlink(path);

    if (bind(bus_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(bus_fd);
        bus_fd = -1;
        return IPC_ERROR;
    }
    if (listen(bus_fd, IPC_BACKLOG) < 0) {
        close(bus_fd);
        bus_fd = -1;
        return IPC_ERROR;
    }
    return IPC_OK;
}

int IPC_send(OBC_Roles_t role_dest, const uint8_t *data, uint16_t length)
{
    if (length > MAX_IPC_PAYLOAD) return -1;

    const char *path = path_for_role(role_dest);
    if (path==NULL) return -1;

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1; /* target role not up (yet), or dead -- fail fast, don't retry-forever */
    }

    IPCFrame frame = { .dest = (uint8_t)role_dest, .src = (uint8_t)my_role, .length = length };
    memcpy(frame.payload, data, length);

    uint8_t wire[4 + MAX_IPC_PAYLOAD];
    int wire_len = ipc_frame_serialize(&frame, wire, sizeof(wire));

    size_t sent = 0;
    while (sent < (size_t)wire_len) {
        ssize_t ret = write(fd, wire + sent, wire_len - sent);
        if (ret < 0) {
            if (errno == EINTR) continue;
            close(fd);
            return -1;
        }
        sent += (size_t)ret;
    }

    close(fd);
}

int IPC_receive(OBC_Roles_t *src_role, uint8_t *buffer, uint16_t max_length)
{
    if (bus_fd < 0) return -1;

    int conn = accept(bus_fd, NULL, NULL); /* blocks */
    if (conn < 0) return -1;

    uint8_t header[4];
    if (read_full(conn, header, 4) < 0) { close(conn); return -1; }

    uint16_t net_len;
    memcpy(&net_len, &header[2], 2);
    uint16_t payload_len = ntohs(net_len);

    if (payload_len > max_length) { close(conn); return -1; }

    if (read_full(conn, buffer, payload_len) < 0) { close(conn); return -1; }

    if (src_role != NULL) {
        *src_role = (OBC_Roles_t)header[1];
    }

    close(conn);
    return payload_len;
}