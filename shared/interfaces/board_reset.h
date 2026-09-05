#ifndef BOARD_RESET_H
#define BOARD_RESET_H

/* Immediately restarts this board, matching what a real hardware reset
   does -- execution resumes from the beginning with everything
   reinitialized. Never returns.

   Two implementations, selected by HW_MODE same as comms_i2c.c:
   platform/sim/drivers/board_reset.c re-execs the current process
   (there's no real hardware to reset in simulation); platform/real/
   drivers/board_reset.c issues an actual Cortex-M system reset. */
void board_reset(void);

#endif // BOARD_RESET_H
