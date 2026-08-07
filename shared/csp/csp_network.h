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
 * is_master: 1 for the OBC (SPI master), 0 for the ADCS (SPI slave).
 * v_bus is medium-agnostic here -- see platform/include/v_bus.h. Which
 * backend actually runs (platform/sim/spi/v_bus.c's Unix socket, or
 * platform/real/spi/spi_driver.c's STM32 SPI peripheral) is decided
 * entirely by the HW_MODE CMake option; this file never changes.
 */
void csp_network_init(uint16_t my_address, int is_master);

#endif
