/*
Connect libcsp to vbus
transmit csp packets through vbus
recieve csp packets from vbus
*/
#include "csp_if_spi.h"

#include <csp/csp_debug.h>
#include <csp/csp_id.h>
#include <string.h>
#include <unistd.h>

#include "../../libs/libcsp/include/csp/csp_types.h"

static int csp_if_spi_tx(csp_iface_t * iface, uint16_t via, csp_packet_t * packet, int from_me) 
{
    (void)via;
    (void)from_me;

    csp_if_spi_conf_t * ifconf = iface->driver_data; // create a interface config

    // TODO: check if full

    csp_id_prepend(packet); // give an id to the packet

    // send packet through the transport
    int ret = ifconf->transport->send(
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
    
    // compute the bytes needed 
    int len = ifconf->transport->receive(
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
    iface->name = "SPI";
    iface->nexthop = csp_if_spi_tx;
    csp_iflist_add(iface);    
}