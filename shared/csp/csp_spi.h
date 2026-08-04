#ifndef CSP_VBUS_H
#define CSP_VBUS_H

#include <csp/csp.h>

#include <pthread.h>
#include <netinet/in.h>

// built for both driver and vbus
typedef struct {
    int (*initialize)(void);

    int (*send)(
        const uint8_t *data,
        uint16_t length
    );

    int (*receive)(
        uint8_t *buffer,
        uint16_t max_length
    );
} CSP_Transport_t;

typedef {
    // Transport abstraction
    CSP_Transport_t *transport;

    // RX Thread
    pthread_t rx_thread;

    // auxillary
    uint8_t chip_select
    uint32_t speed;

} csp_if_spi_conf_t;

void csp_vbus_init(csp_iface_t * iface, csp_if_spi_conf_t * ifconf)

#endif