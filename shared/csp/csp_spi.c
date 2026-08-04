/*
Connect libcsp to vbus
transmit csp packets through vbus
recieve csp packets from vbus
*/
#include "csp_spi.h"

#include <_time.h>
#include <csp/csp_debug.h>
#include <string.h>

#include "../../platform/sim/include/v_bus.h"
#include "../../libs/libcsp/include/csp/csp_types.h"

static int csp_if_spi_tx(csp_iface_t * iface, csp_packet_t * packet) 
{
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

// check if we recieved anything
static int csp_if_spi_rx_work(
    csp_iface_t *iface
)
{
    csp_if_spi_conf_t *ifconf = iface->driver_data; // makes config interface

    csp_packet_t *packet = csp_buffer_get(0); // this is space in buffer we grab
    if (packet == NULL)
    {
        return CSP_ERR_NOMEN; // check we actually got a packet in buffer
    }

    int header_size = csp_id_setup_rx(packet);
    
    // compute the bytes needed 
    int len = iconf->transport->receive(
        packet->frame_begin,
        sizeof(packet->data) + header_size
    );

    // set the frame length
    packet->frame_length = len;

    // parse the frame and strip the ID field
    if(csp_id_strip(packet)!=0)
    {
        // if strip fails...
        csp_buffer_free(packet);
        return CSP_ERR_INVAL
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
        csp_if_spi_rx_work(iface);
    }

    return NULL;
}

void csp_if_spi_init(csp_iface_t * iface, csp_if_spi_conf_t * ifconf)
{
    pthread_attr_t attributes;
    int ret;

    iface->driver_data = ifconf; // setting that ifconf that we use above

    ret = pthread_create(&ifconf->rx_thread, &attributes, csp_if_spi_rx_loop, iface)
    if (ret != 0) {
        csp_print("csp_if_spi_init: pthread_create failed: %s: %d\n", strerror(ret), ret);
    }

    // register the interface
    iface->name = "SPI";
    iface->nexthop = csp_if_spi_tx;
    csp_iflist_add(iface);
}