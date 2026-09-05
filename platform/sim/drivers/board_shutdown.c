#include "board_shutdown.h"
#include <stdio.h>
#include <unistd.h>

void board_shutdown(void)
{
    printf("[BOARD SHUTDOWN] Halting -- will not restart automatically\n");
    fflush(stdout);
    _exit(0); // ends the whole process, not just this thread -- see why above
}