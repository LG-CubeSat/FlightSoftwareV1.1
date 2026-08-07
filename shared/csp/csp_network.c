#include "csp_network.h"
#include "csp_if_spi.h"

#include <csp/csp.h>

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include "v_bus.h"

static csp_iface_t csp_spi_iface;
static csp_if_spi_conf_t csp_spi_conf;
static CSP_Transport_t csp_spi_transport;
static VBus_t csp_v_bus;

/*
 * csp_if_spi.c never calls transport->initialize() -- v_bus is initialized
 * directly below, before the interface is registered, so this only needs
 * to satisfy the CSP_Transport_t struct.
 */
static int csp_transport_initialize_noop(void)
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
    csp_v_bus = create_v_bus();

    if (csp_v_bus.initialize(is_master) != V_BUS_OK) {
        fprintf(stderr, "[CSP] v_bus initialize failed\n");
        fflush(stderr);
        return;
    }

    csp_spi_transport.initialize = csp_transport_initialize_noop;
    csp_spi_transport.send = csp_v_bus.send;
    csp_spi_transport.receive = csp_v_bus.receive;

    csp_spi_conf.transport = &csp_spi_transport;

    csp_init();

    /* Single-interface node: no routing table (CSP_USE_RTABLE is off), so
     * marking this interface default is the entire routing setup needed. */
    csp_spi_iface.addr = my_address;
    csp_spi_iface.is_default = 1;

    csp_if_spi_init(&csp_spi_iface, &csp_spi_conf);

    pthread_t router_thread;
    int ret = pthread_create(&router_thread, NULL, csp_router_thread, NULL);
    if (ret != 0) {
        fprintf(stderr, "[CSP] router thread create failed: %d\n", ret);
        fflush(stderr);
    }

    printf("[CSP] Network initialized: address=%u\n", my_address);
    fflush(stdout);
}
