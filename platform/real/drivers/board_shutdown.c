#include "board_shutdown.h"

void board_shutdown(void)
{
    __asm volatile ("cpsid i"); // mask interrupts
    for (;;) {
        __asm volatile ("wfi"); // low-power halt until an external event
    }
}