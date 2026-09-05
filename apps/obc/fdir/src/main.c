/*
Jobs:
Limit checking all the telemetry the OBC receives for rotten values
Health monitor across each subsystem
Watch dog
fall back: deciedes what mode to actually fall back to if something is wrong

Basically it's job is to diagnose the issue and deciede what should be done
Then it hands it off to the Supervisor

*/

#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#include "obc_ipc.h"
#include "watchdog.h"
#include "fallback.h"

int main(void) {
    printf("[OBC FDIR] Initializing.\n");

    /* Init here */
    IPC_initialize(ROLE_FDIR);

    if (watchdog_thread_init() != 0) {
        printf("[OBC FDIR] Failed to launch the Watchdog thread.\n");
    }

    /* TEMPORARY TEST HOOK -- proves FDIR can trigger an external board
       reset through fallback_reset_board -> commands' relay -> CSP.
       Remove once real fault detection calls this instead. */
    sleep(3); // let commands finish connecting to ADCS first
    if (fallback_reset_board(ADCS_ADDRESS, ADCS_CMD_PORT) != 0) {
        printf("[OBC FDIR] TEST: reset request failed to send\n");
    } else {
        printf("[OBC FDIR] TEST: sent CMD_RESET for ADCS via fallback\n");
    }
    fflush(stdout);

    for (;;) {
        sleep(1);
    }

    return 0;
}
