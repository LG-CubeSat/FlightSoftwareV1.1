#include <stdio.h>
#include <unistd.h>

#include "obc_ipc.h"
#include "time_sync.h"
#include "heartbeat.h"

int main(void) {
    printf("[OBC TIME] Initializing.\n");
    fflush(stdout);

    if (IPC_initialize(ROLE_TIME) != 0) {
        printf("[OBC TIME] Failed to init IPC.\n");
        return 1;
    }

    time_sync_broadcast_thread_init();
    time_sync_request_thread_init();
    heartbeat_thread_init();

    for (;;) sleep(1);
    return 0;
}
