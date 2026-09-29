#ifndef RADIO_PINS_H
#define RADIO_PINS_H

#include <termios.h>

/*
E22-400T30D wiring -- placeholders until the OBC PCB pinout is confirmed
(see docs/satellite_architecture.md's Open Items / TBD). Update these once
the board is in hand, same pattern as platform/real/include/i2c_addresses.h.
*/

// Pi Zero 2 W needs enable_uart=1 + dtoverlay=disable-bt in config.txt first
// (frees the full PL011 UART instead of the mini-UART shared with Bluetooth) --
// see docs/satellite_architecture.md's Path to HW_MODE, item C.5.
#define RADIO_UART_DEVICE "/dev/ttyAMA0" // TODO: confirm against real wiring

// E22 modules ship with serial baud 9600 by factory default -- confirm this
// wasn't reconfigured away from default before relying on it.
#define RADIO_UART_BAUD B9600 // TODO: confirm against the module's configured serial baud rate

#define RADIO_GPIO_CHIP "gpiochip0" // TODO: confirm against real wiring
#define RADIO_GPIO_LINE_M0  5  // TODO: placeholder GPIO line, confirm real pin
#define RADIO_GPIO_LINE_M1  6  // TODO: placeholder GPIO line, confirm real pin
#define RADIO_GPIO_LINE_AUX 13 // TODO: placeholder GPIO line, confirm real pin

#endif
