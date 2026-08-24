#ifndef CSP_IF_SPI_H
#define CSP_IF_SPI_H

#include <csp/csp.h>

#include <pthread.h>
#include <netinet/in.h>

// Transport-agnostic: any comms_bus backend (SIM or real) plugs in here
// via these three function pointers, so this layer never assumes a medium.
typedef struct {
    int (*initialize)(void);

    int (*send)(
        uint8_t dest_addr,
        const uint8_t *data,
        uint16_t length
    );

    int (*receive)(
        uint8_t *src_addr_out,
        uint8_t *buffer,
        uint16_t max_length
    );
} CSP_Transport_t;

typedef struct {
    // Transport abstraction
    CSP_Transport_t *transport;

    // RX Thread
    pthread_t rx_thread;

} csp_if_spi_conf_t;

void csp_if_spi_init(csp_iface_t * iface, csp_if_spi_conf_t * ifconf);

#endif