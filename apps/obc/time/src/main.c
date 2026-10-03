#include <stdio.h>
#include <unistd.h>

#include "obc_ipc.h"
#include "time_sync.h"
#include "heartbeat.h"
#include "time_health.h"

int main(void) {
    printf("[OBC TIME] Initializing.\n");
    fflush(stdout);

    if (IPC_initialize(ROLE_TIME) != 0) {
        printf("[OBC TIME] Failed to init IPC.\n");
        return 1;
    }

    if (time_health_init() != 0) {
        fprintf(stderr, "[OBC TIME] Failed to initialize health tracking.\n");
        return 1;
    }

    if (time_sync_broadcast_thread_init() != 0 ||
        time_sync_request_thread_init() != 0) {
        fprintf(stderr, "[OBC TIME] Failed to start a worker thread.\n");
        return 1;
    }

    if (heartbeat_thread_init() != 0) {
        fprintf(stderr, "[OBC TIME] Failed to start heartbeat thread.\n");
        return 1;
    }

    for (;;) sleep(1);
    return 0;
}
