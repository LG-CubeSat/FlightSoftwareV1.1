/*
Init libcsp
Set node adress
configuring routing
initialize interface
start csp
*/
#ifndef CSP_NETWORK_H
#define CSP_NETWORK_H

#include <stdint.h>

/*
 * Brings up CSP for this process: initializes the v_bus transport
 * (as master or slave), registers it as the single default CSP
 * interface under my_address, and starts the background router
 * thread that drains incoming packets to bound sockets.
 *
 * is_master: 1 for the OBC (v_bus socket server), 0 for the ADCS
 * (v_bus socket client). See platform/sim/spi/v_bus.c.
 */
void csp_network_init(uint16_t my_address, int is_master);

#endif
