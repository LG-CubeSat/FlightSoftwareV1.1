#include "v_bus.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define SOCKET_PATH "/tmp/v_bus.sock"

static int bus_fd = -1;

VBus_t create_v_bus(void)
{
    VBus_t v_bus;
    v_bus.initialize = &v_bus_initialize;
    v_bus.send = &v_bus_send;
    v_bus.receive = &v_bus_receive;

    return v_bus;
}

VBusStatus_t v_bus_initialize(int is_master)
{
    struct sockaddr_un addr; // declare the adress card

    bus_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (bus_fd < 0) {
        return V_BUS_ERROR;
    }

    // fill out the address card
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (is_master) {
        // OBC logic
        unlink(SOCKET_PATH); // clear old socket file

        // bind to the adress
        if (bind(bus_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            return V_BUS_ERROR;
        }

        listen(bus_fd, 1);
        printf("[V-BUS Master waiting for Slave...\n]");

        int connection_fd = accept(bus_fd, NULL, NULL); // Wait for ADCS
        if (connection_fd < 0) return V_BUS_ERROR;

        close(bus_fd); // we don't needthe listen anymore
        bus_fd = connection_fd; // use the actual connection for future sends
    } else {
        // slave logic (ADCS)
        printf("[V-BUS] Slave connecting..\n");
        // Loop until the OBC (master) is ready
        while (connect(bus_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            usleep(100000); // wait 100ms and try again
        }
    }

    return V_BUS_OK;
}

int v_bus_send(const uint8_t *data, uint16_t length)
{
    if (bus_fd < 0) return -1;
    // write() sends 'length' bytes from 'data' through the socket 'bus_fd'
    return (int)write(bus_fd, data, length);
}

int v_bus_receive(uint8_t *buffer, uint16_t max_length) 
{
    if (bus_fd < 0) return -1;
    // read() blocks until the data arrives and fills 'buffer' up to 'max_length'
    return (int)read(bus_fd, buffer, max_length);
}