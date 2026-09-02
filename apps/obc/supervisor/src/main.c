#include <stdio.h>
#include "supervisor.h"
#include "processes.h"
#include <unistd.h>

int main(void) {
    if (init_supervisor() != 0) {
        printf("[OBC SUPERVISOR] Failed to launch other sub-processes.\n");
        return 1;
    }

    if (init_heartbeat_thread() != 0) {
        printf("[OBC SUPERVISOR] Failed to launch the Heartbeat thread.\n");
    }

    if (init_shutdown_thread() != 0) {
        printf("[OBC SUPERVISOR] Failed to launch the Shutdown thread\n.");
    }

    for (;;) {
        sleep(1); // crude loop, TODO: make a clean SIGTERM Shutdown
    }

    return 0;
}
