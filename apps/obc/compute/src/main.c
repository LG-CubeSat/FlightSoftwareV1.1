#include <stdio.h>
#include <unistd.h>

#include "obc_ipc.h"
#include "worker.h"
#include "dispatch.h"
#include "heartbeat.h"
#include "compute_health.h"

int main(void) {
    printf("[OBC COMPUTE] Initializing.\n");
    fflush(stdout);

    if (IPC_initialize(ROLE_COMPUTE) != 0) {
        printf("[OBC COMPUTE] Failed to init IPC.\n");
        return 1;
    }

    if (compute_health_init() != 0) {
        fprintf(stderr, "[OBC COMPUTE] Failed to initialize health tracking.\n");
        return 1;
    }

    if (dispatch_thread_init() != 0) {
        fprintf(stderr, "[OBC COMPUTE] Failed to start dispatch thread.\n");
        return 1;
    }
    
    if (heartbeat_thread_init() != 0) {
        fprintf(stderr, "[OBC COMPUTE] Failed to start heart thread.\n");
        return 1;
    }

    for (;;) sleep(1);
    return 0;
}
