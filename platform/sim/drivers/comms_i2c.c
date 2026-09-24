#include "comms_bus.h"
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

#define SOCKET_PATH "/tmp/comms_i2c.sock"
#define BACKLOG 10
#define MAX_CONNECTIONS 5

static uint8_t my_bus_address;
static uint8_t bus_is_master;

static int bus_fd = -1;
static int connections_fd[MAX_CONNECTIONS];
static int connection_count = 0;

pthread_mutex_t connection_lock;

/*
 * SOCK_STREAM doesn't preserve message boundaries -- one read() can return
 * a partial frame, several frames concatenated together, or anything in
 * between. Each connection needs its own leftover buffer so bytes read
 * past the end of "this" frame aren't lost before the next receive() call.
 * Slot 0 is the slave's single connection; for the master, index by
 * position in connections_fd[].
 */
#define LEFTOVER_BUF_SIZE (2 * (4 + MAX_FRAME_PAYLOAD))
typedef struct {
    uint8_t buf[LEFTOVER_BUF_SIZE];
    int len; // valid bytes currently sitting in buf, not yet consumed into a frame
} leftover_t;

static leftover_t slave_leftover;
static leftover_t master_leftover[MAX_CONNECTIONS];

/* A board simulating a hardware reset closes its end of the connection
   (see fault_management.c's CLOEXEC'd bus_fd) -- write()/read() on that
   slot then fails, and it needs to stay dead (not retried forever) while
   NOT stopping delivery to any later, still-live slot. connections_fd is
   append-only by design (see comms_bus_receive's comment on indexing),
   so this leaves a hole rather than compacting -- callers must treat -1
   as "skip this slot", not "no more slots after this one". */
static void mark_connection_dead(int index)
{
    pthread_mutex_lock(&connection_lock);
    if (connections_fd[index] != -1) {
        close(connections_fd[index]);
        connections_fd[index] = -1;
    }
    pthread_mutex_unlock(&connection_lock);
}

CommsBus_t create_comms_bus(void)
{
    CommsBus_t bus;
    bus.initialize = &comms_bus_initialize;
    bus.send = &comms_bus_send;
    bus.receive = &comms_bus_receive;

    return bus;
}

static void * loop_accept_new_connections(void * param)
{
    (void) param;

    while (1) {
        int connection_fd = accept(bus_fd, NULL, NULL); // Wait for ADCS
        if (connection_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No connection waiting yet -- sleep briefly instead of
                // busy-spinning. A tight loop here starves other threads
                // (the main thread's send/receive, the slave process) of
                // CPU time on constrained hosts, which can delay the real
                // connection from ever completing.
                usleep(5000); // 5ms
            } else {
                fprintf(stderr, "[COMMS BUS] accept() failed: %s\n", strerror(errno));
                fflush(stderr);
            }
            continue;
        }

        // The accepted connection can inherit O_NONBLOCK from the
        // listening socket on this platform -- only the listener should
        // be non-blocking; comms_bus_send/receive assume a blocking
        // connection, so restore that here.
        int conn_flags = fcntl(connection_fd, F_GETFL, 0);
        fcntl(connection_fd, F_SETFL, conn_flags & ~O_NONBLOCK);
        
        pthread_mutex_lock(&connection_lock);

        connections_fd[connection_count] = connection_fd; // use the actual connection for future sends
        connection_count++;

        pthread_mutex_unlock(&connection_lock);

        printf("[COMMS BUS] Master accepted connection from Slave. Connection Count: %d\n", connection_count);
        fflush(stdout);
    }
    
    // clean up mutex recoruses
    pthread_mutex_destroy(&connection_lock);

    return NULL;
}

CommsBusStatus_t comms_bus_initialize(uint8_t my_address, int is_master)
{
    // store static variables
    my_bus_address = my_address;
    bus_is_master = is_master;

    struct sockaddr_un addr; // declare the adress card

    /* SOCK_STREAM: SOCK_SEQPACKET is not implemented for AF_UNIX on macOS
     * (socket() fails with EPROTONOSUPPORT there), so it cannot be used here. */
    bus_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (bus_fd < 0) {
        fprintf(stderr, "[COMMS BUS] socket() failed: %s\n", strerror(errno));
        fflush(stderr);
        return COMMS_BUS_ERROR;
    }
    // Without this, a board simulating a hardware reset via execv() would
    // leak this socket into the "fresh" process instead of it actually
    // being gone, since execv only closes fds marked close-on-exec.
    fcntl(bus_fd, F_SETFD, FD_CLOEXEC);

    // fill out the address card
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (is_master) {
        // set connections array to all -1
        memset(connections_fd, -1, sizeof(connections_fd));

        // OBC logic
        unlink(SOCKET_PATH); // clear old socket file

        // bind to the address
        if (bind(bus_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            fprintf(stderr, "[COMMS BUS] bind() failed: %s\n", strerror(errno));
            fflush(stderr);
            return COMMS_BUS_ERROR;
        }

        listen(bus_fd, BACKLOG);
        printf("[COMMS BUS] Master waiting for Slave...\n");
        fflush(stdout);

        // Only the listening socket needs non-blocking accept() -- the
        // slave's connect() below must stay on its normal blocking socket.
        int flags = fcntl(bus_fd, F_GETFL, 0);
        fcntl(bus_fd, F_SETFL, flags | O_NONBLOCK);

        if (pthread_mutex_init(&connection_lock, NULL) != 0) {
            printf("[COMMS BUS] Mutex initialization failed.\n");
            return COMMS_BUS_ERROR;
        }

        pthread_t connection_thread;
        int ret = pthread_create(&connection_thread, NULL, loop_accept_new_connections, NULL);
        if (ret != 0) {
            fprintf(stderr, "[COMMS BUS] Failed to create connection pthread: %d\n", ret);
            fflush(stderr);
            return COMMS_BUS_ERROR;
        }
    } else {
        // slave logic (ADCS)
        printf("[COMMS BUS] Slave connecting..\n");
        fflush(stdout);
        // Loop until the OBC (master) is ready
        while (connect(bus_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            usleep(100000); // wait 100ms and try again
        }
        printf("[COMMS BUS] Slave connected to Master.\n");
        fflush(stdout);
    }

    return COMMS_BUS_OK;
}

// Reassembles exactly one frame from 'fd', buffering across read() calls
// as needed -- a stream socket can hand back a partial frame, several
// frames concatenated together, or anything in between. Blocks until one
// full frame is available or a hard error/disconnect occurs. Any bytes
// belonging to the *next* frame are kept in 'lo' for the following call,
// not discarded.
static int receive_one_frame(int fd, leftover_t *lo, Frame *frame_out)
{
    while (1) {
        if (lo->len >= 4) {
            uint16_t net_length;
            memcpy(&net_length, &lo->buf[2], sizeof(net_length));
            int needed = 4 + ntohs(net_length);

            if (needed > LEFTOVER_BUF_SIZE) {
                fprintf(stderr, "[COMMS BUS] frame claims %d bytes, exceeds buffer -- dropping connection\n", needed);
                fflush(stderr);
                return -1;
            }

            if (lo->len >= needed) {
                if (frame_deserialize(lo->buf, lo->len, frame_out) < 0) {
                    return -1;
                }
                // Shift any bytes belonging to the next frame down to the
                // front so they aren't lost before the next call.
                int remaining = lo->len - needed;
                memmove(lo->buf, lo->buf + needed, remaining);
                lo->len = remaining;
                return frame_out->length;
            }
        }

        int ret;
        do {
            ret = (int)read(fd, lo->buf + lo->len, LEFTOVER_BUF_SIZE - lo->len);
        } while (ret < 0 && errno == EINTR); // retry on benign signal interruption

        if (ret < 0) {
            fprintf(stderr, "[COMMS BUS] read() failed: %s\n", strerror(errno));
            fflush(stderr);
            return -1;
        }
        if (ret == 0) {
            return -1; // peer closed the connection
        }
        lo->len += ret;
    }
}

int comms_bus_send(uint8_t dest_addr, const uint8_t *data, uint16_t length)
{
    if (bus_fd < 0) return -1;
    // write() sends 'length' bytes from 'data' through the socket 'bus_fd'
    
    // creating a frame
    Frame frame;
    frame.dest_addr = dest_addr;
    frame.src_addr = my_bus_address;

    if (length > MAX_FRAME_PAYLOAD) {
        printf("[COMMS BUS] Frame creation failed because payload length of %d exceeded limit of %d", 
            length, 
            MAX_FRAME_PAYLOAD
        );
        return -1;
    }
    
    frame.length = length;
    memcpy(frame.payload, data, length); // set the frame's payload

    uint8_t wire_buf[4 + MAX_FRAME_PAYLOAD];
    int wire_len = frame_serialize(&frame, wire_buf, sizeof(wire_buf));

    int ret = -1;
    if (bus_is_master) {
        // comms_bus_initialize() returns as soon as the accept thread is
        // spawned, before that thread has necessarily accepted anyone --
        // give it a brief window to catch up rather than silently sending
        // to nobody. Mirrors the slave's own connect() retry loop below.
        int wait_attempts = 0;
        while (connection_count == 0 && wait_attempts < 50) {
            usleep(20000); // 20ms
            wait_attempts++;
        }

        // Snapshot under the lock, then release before doing any blocking
        // I/O -- holding the lock across write() would block the accept
        // thread from registering new connections for as long as any one
        // write takes.
        pthread_mutex_lock(&connection_lock);
        int snapshot_count = connection_count;
        int snapshot_fds[MAX_CONNECTIONS];
        memcpy(snapshot_fds, connections_fd, sizeof(snapshot_fds));
        pthread_mutex_unlock(&connection_lock);

        for (int i=0; i < snapshot_count; i++) {
            if (snapshot_fds[i] == -1) {
                continue; // this slot's connection already died -- skip, more may follow
            }

            do {
                ret = (int)write(snapshot_fds[i], wire_buf, wire_len);
            } while (ret < 0 && errno == EINTR); // retry on benign signal interruption
            if (ret < 0) {
                fprintf(stderr, "[COMMS BUS] write() failed: %s\n", strerror(errno));
                fflush(stderr);
                mark_connection_dead(i);
            }
        }
    } else {
        do {
            ret = (int)write(bus_fd, wire_buf, wire_len);
        } while (ret < 0 && errno == EINTR); // retry on benign signal interruption
        if (ret < 0) {
            fprintf(stderr, "[COMMS BUS] write() failed: %s\n", strerror(errno));
            fflush(stderr);
        }
    }

    // Report success in terms of the caller's own payload length, not the
    // wire length -- framing (the 4-byte header) is an internal detail of
    // this layer and shouldn't leak into what "bytes sent" means to callers.
    if (ret == wire_len) {
        return length;
    }
    return -1;
}

int comms_bus_receive(uint8_t *src_addr_out, uint8_t *buffer, uint16_t max_length)
{
    (void)max_length; // frame.payload is already bounded to MAX_FRAME_PAYLOAD by frame_deserialize
    int found = 0;
    Frame frame;

    if (bus_is_master) {

        // Same snapshot-then-release pattern as comms_bus_send -- don't
        // hold connection_lock across a blocking read().
        pthread_mutex_lock(&connection_lock);
        int snapshot_count = connection_count;
        int snapshot_fds[MAX_CONNECTIONS];
        memcpy(snapshot_fds, connections_fd, sizeof(snapshot_fds));
        pthread_mutex_unlock(&connection_lock);

        for (int i=0; i < snapshot_count; i++) {
            if (snapshot_fds[i] == -1) {
                continue; // this slot's connection already died -- skip, more may follow
            }

            // Indexed by connection slot, not fd -- connections_fd only
            // ever appends (dead slots are nulled in place, not removed),
            // so slot i is always the same physical connection across
            // calls, matching master_leftover[i].
            if (receive_one_frame(snapshot_fds[i], &master_leftover[i], &frame) < 0) {
                mark_connection_dead(i);
                continue; // this connection's read failed -- try the next
            }
            if (frame.dest_addr != my_bus_address) {
                continue; // not addressed to us -- discard, per the broadcast/filter design
            }
            found = 1;
            break;
        }
    } else {
        if (bus_fd < 0) return -1;
        if (receive_one_frame(bus_fd, &slave_leftover, &frame) < 0) {
            return -1;
        }
        if (frame.dest_addr != my_bus_address) {
            return 0; // not for us
        }
        found = 1;
    }

    if (!found) {
        return -1;
    }

    if (src_addr_out != NULL) {
        *src_addr_out = frame.src_addr;
    }
    memcpy(buffer, frame.payload, frame.length);
    return frame.length;
}
