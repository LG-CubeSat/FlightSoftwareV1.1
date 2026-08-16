#include "comms_i2c.h"

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

#define SOCKET_PATH "/tmp/comms_i2c.sock"
#define BACKLOG 10
#define MAX_CONNECTIONS 5

static int bus_fd = -1;
static int connections_fd[MAX_CONNECTIONS];
static int connection_count = 0;

pthread_mutex_t connection_lock;

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
                // no connections
                printf("[COMMS BUS] No clients waiting. Continuing...\n");
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
    struct sockaddr_un addr; // declare the adress card

    /* SOCK_STREAM: SOCK_SEQPACKET is not implemented for AF_UNIX on macOS
     * (socket() fails with EPROTONOSUPPORT there), so it cannot be used here. */
    bus_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (bus_fd < 0) {
        fprintf(stderr, "[COMMS BUS] socket() failed: %s\n", strerror(errno));
        fflush(stderr);
        return COMMS_BUS_ERROR;
    }

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

        pthread_t connection_thread;
        int ret = pthread_create(&connection_thread, NULL, loop_accept_new_connections, NULL);
        if (ret != 0) {
            fprintf(stderr, "[COMMS BUS] Failed to create connection pthread: %d\n", ret);
            fflush(stderr);
            return COMMS_BUS_ERROR;
        }

        if (pthread_mutex_init(&connection_lock, NULL) != 0) {
            printf("[COMMS BUS] Mutex initialization failed.\n");
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

int comms_check_new_connections(int bus_fd)
{
    int connection_fd = accept(bus_fd, NULL, NULL); // Wait for ADCS
    if (connection_fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // no connections
            printf("[COMMS BUS] No clients waiting. Continuing...\n");
        } else {
            fprintf(stderr, "[COMMS BUS] accept() failed: %s\n", strerror(errno));
            fflush(stderr);
            return COMMS_BUS_ERROR;
        }
    }
    // add new connection
    connections_fd[connection_count] = connection_fd;
    connection_fd++;

    printf("[COMMS BUS] Master accepted connection from Slave. Connection Count: %d\n", connection_count);
    return COMMS_BUS_OK;
}

int comms_bus_send(uint8_t dest_addr, const uint8_t *data, uint16_t length)
{
    if (bus_fd < 0) return -1;
    // write() sends 'length' bytes from 'data' through the socket 'bus_fd'
    int ret;
    do {
        ret = (int)write(bus_fd, data, length);
    } while (ret < 0 && errno == EINTR); // retry on benign signal interruption
    if (ret < 0) {
        fprintf(stderr, "[COMMS BUS] write() failed: %s\n", strerror(errno));
        fflush(stderr);
    }
    return ret;
}

int comms_bus_receive(uint8_t *src_addr_out, uint8_t *buffer, uint16_t max_length)
{
    if (bus_fd < 0) return -1;
    // read() blocks until the data arrives and fills 'buffer' up to 'max_length'
    int ret;
    do {
        ret = (int)read(bus_fd, buffer, max_length);
    } while (ret < 0 && errno == EINTR); // retry on benign signal interruption
    if (ret < 0) {
        fprintf(stderr, "[COMMS BUS] read() failed: %s\n", strerror(errno));
        fflush(stderr);
    }
    return ret;
}
