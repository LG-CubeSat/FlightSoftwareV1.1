#include "csp_network.h"
#include "csp_if_comms_bus.h"

#include <csp/csp.h>

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include "comms_bus.h"

static csp_iface_t csp_comms_bus_iface;
static CspCommsBusConfig_t csp_comms_bus_config;
static CspCommsBusTransport_t csp_comms_bus_transport;
static CommsBus_t comms_bus;

/*
 * csp_if_comms_bus.c never calls transport->initialize() -- the comms bus is
 * initialized directly below, before the interface is registered, so this
 * only needs to satisfy the CspCommsBusTransport_t struct.
 */
static int csp_comms_bus_transport_initialize(void)
{
    return 0;
}

/* Router pump: no csp_route_start() exists in this libcsp build, so the
 * application must call csp_route_work() regularly itself. */
static void * csp_router_thread(void * param)
{
    (void) param;

    while (1) {
        csp_route_work();
        usleep(1000);
    }

    return NULL;
}

void csp_network_init(uint16_t my_address, int is_master)
{
    comms_bus = create_comms_bus();

    if (comms_bus.initialize(my_address, is_master) != COMMS_BUS_OK) {
        fprintf(stderr, "[CSP] comms bus initialize failed\n");
        fflush(stderr);
        return;
    }

    csp_comms_bus_transport.initialize = csp_comms_bus_transport_initialize;
    csp_comms_bus_transport.send = comms_bus.send;
    csp_comms_bus_transport.receive = comms_bus.receive;

    csp_comms_bus_config.transport = &csp_comms_bus_transport;

    csp_init();

    /* Single-interface node: no routing table (CSP_USE_RTABLE is off), so
     * marking this interface default is the entire routing setup needed. */
    csp_comms_bus_iface.addr = my_address;
    csp_comms_bus_iface.is_default = 1;

    csp_if_comms_bus_init(&csp_comms_bus_iface, &csp_comms_bus_config);

    pthread_t router_thread;
    int ret = pthread_create(&router_thread, NULL, csp_router_thread, NULL);
    if (ret != 0) {
        fprintf(stderr, "[CSP] router thread create failed: %d\n", ret);
        fflush(stderr);
    }

    printf("[CSP] Network initialized: address=%u\n", my_address);
    fflush(stdout);
}
