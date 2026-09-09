#include <stdio.h>
#include <unistd.h>

#include "obc_ipc.h"
#include "compute.h"
#include "heartbeat.h"

int main(void) {
    printf("[OBC COMPUTE] Initializing.\n");
    fflush(stdout);

    if (IPC_initialize(ROLE_COMPUTE) != 0) {
        printf("[OBC COMPUTE] Failed to init IPC.\n");
        return 1;
    }

    dispatch_thread_init();
    heartbeat_thread_init();

    for (;;) sleep(1);
    return 0;
}
