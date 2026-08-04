/*
Connect libcsp to vbus
transmit csp packets through vbus
recieve csp packets from vbus
*/
#include "csp_spi.h"

#include <csp/csp_debug.h>
#include <string.h>

#include "../../platform/sim/include/v_bus.h"
#include "../../libs/libcsp/include/csp/csp_types.h"

static int csp_if_spi_tx(csp_iface_t * iface, uint16_t via, csp_packet_t * packet, int from_me) {
    // avoid compiler warning about unused params
    (void)via;
    (void)from_me;

    csp_if_spi_conf_t * ifconf = iface->driver_data; // create a interface config

    // TODO: check if full

    cspi_id_prepend(packet); // give an id to the packet

    // send packet through the transport
    ifconf->transport->send(
        packet->frame_begin,
        packet->frame_length
    );

    // free up buffer space
    csp_buffer_free(packet);

    return CSP_ERR_NONE; // we successfully sent a packet
}

