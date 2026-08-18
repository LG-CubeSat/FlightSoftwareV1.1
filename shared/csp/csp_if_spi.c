/*
Bridges libcsp to the underlying comms_bus transport (see comms_bus.h) --
sends/receives csp packets through whichever CSP_Transport_t was configured
(SIM or real), without this layer ever knowing or caring which one it is.
*/
#include "csp_if_spi.h"

#include <csp/csp_debug.h>
#include <csp/csp_id.h>
#include <string.h>
#include <unistd.h>

#include "../../libs/libcsp/include/csp/csp_types.h"

static int csp_if_spi_tx(csp_iface_t * iface, uint16_t via, csp_packet_t * packet, int from_me)
{
    (void)from_me;
    (void)via; // CSP_NO_VIA_ADDRESS with no routing table configured -- not a real address, see packet->id.dst below

    csp_if_spi_conf_t * ifconf = iface->driver_data; // create a interface config

    // TODO: check if full

    // Capture the packet's own destination before csp_id_prepend() packs
    // the header into the wire bytes -- 'via' is CSP_NO_VIA_ADDRESS here
    // (no routing table means CSP never computes a real next-hop), so the
    // packet's own CPU-readable destination field is the real address.
    uint8_t dest_addr = (uint8_t)packet->id.dst;

    csp_id_prepend(packet); // give an id to the packet

    // send packet through the transport
    int ret = ifconf->transport->send(
        dest_addr,
        packet->frame_begin,
        packet->frame_length
    );

    // free up buffer space
    csp_buffer_free(packet);

    if (ret < 0) {
        iface->tx_error++;
        return CSP_ERR_TX;
    }

    return CSP_ERR_NONE; // we successfully sent a packet
}

// check if we recieved anything
static int csp_if_spi_rx_work(
    csp_iface_t *iface
)
{
    csp_if_spi_conf_t *ifconf = iface->driver_data; // makes config interface

    csp_packet_t *packet = csp_buffer_get(0); // this is space in buffer we grab
    if (packet == NULL)
    {
        return CSP_ERR_NOMEM; // check we actually got a packet in buffer
    }

    int header_size = csp_id_setup_rx(packet);

    // compute the bytes needed -- src_addr is who the bus says sent this
    // frame; not used for CSP routing (CSP's own header carries that), only
    // needed to satisfy the addressed comms_bus contract
    uint8_t src_addr;
    int len = ifconf->transport->receive(
        &src_addr,
        packet->frame_begin,
        sizeof(packet->data) + header_size
    );

    // len is not a real number
    if (len < 0) {
        csp_buffer_free(packet);
        return CSP_ERR_INVAL;
    }

    // len is too short to be real (less then minimum header...)
    if (len < header_size) {
        csp_buffer_free(packet);
        return CSP_ERR_INVAL;
    }

    // set the frame length
    packet->frame_length = len;

    // parse the frame and strip the ID field
    if(csp_id_strip(packet)!=0)
    {
        // if strip fails...
        csp_buffer_free(packet);
        return CSP_ERR_INVAL;
    }

    /* 
    qfifo stands for queue first in, first out
    hands off to the queue for the router to pick up
    the queue holds memory adresses to packets/iface
    both queue and router are internal to the libcsp
    router is the one that calls the TX
    */
    csp_qfifo_write(
        packet,
        iface,
        NULL // normal thread/task and not interrupt service routine
    );

    return CSP_ERR_NONE;
}

static void * csp_if_spi_rx_loop(void * param) 
{
    csp_iface_t *iface = param;
    
    while(1)
    {
        int ret = csp_if_spi_rx_work(iface);

        if (ret == CSP_ERR_NOMEM) {
            usleep(10000); // if recieve is blocking
        }
        else if (ret != CSP_ERR_NONE) {
            iface->rx_error++;
        }
    }

    return NULL;
}

void csp_if_spi_init(csp_iface_t * iface, csp_if_spi_conf_t * ifconf)
{
    pthread_attr_t attributes;
    int ret;

    iface->driver_data = ifconf; // setting that ifconf that we use above

    ret = pthread_create(&ifconf->rx_thread, NULL, csp_if_spi_rx_loop, iface);
    if (ret != 0) {
        csp_print("csp_if_spi_init: pthread_create failed: %s: %d\n", strerror(ret), ret);
    }

    // note: we let the csp_network initialize the transport

    // register the interface
    iface->name = "COMMS";
    iface->nexthop = csp_if_spi_tx;
    csp_iflist_add(iface);    
}