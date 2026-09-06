#include "board_reset.h"

#include <stdint.h>

/* ARM Cortex-M Application Interrupt and Reset Control Register.
   SYSRESETREQ is part of the ARMv7-M architecture itself (not
   vendor-specific), so this works on any Cortex-M part without
   needing a CMSIS/HAL dependency -- same raw-register style already
   used by the FreeRTOS hardware port for this board, see the
   portNVIC_* registers in rtos/ports/hw/adcs/port.c. */
#define AIRCR (*(volatile uint32_t *)0xE000ED0CUL)
#define AIRCR_VECTKEY (0x5FAUL << 16)
#define AIRCR_SYSRESETREQ (1UL << 2)

void board_reset(void)
{
    AIRCR = AIRCR_VECTKEY | AIRCR_SYSRESETREQ;
    for (;;) {
        // reset takes effect within a few clock cycles; nothing to do
        // but wait for it, execution never reaches past this point
    }
}
