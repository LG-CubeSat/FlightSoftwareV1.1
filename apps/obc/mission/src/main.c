#include "stdio.h"
#include "obc_ipc.h"
#include "unistd.h"
#include "scheduler.h"
#include "heartbeat.h"
#include "autonomy.h"
#include "mission_health.h"

int main(void) {
    printf("[OBC MISSION] Initializing.\n");
    fflush(stdout);

    if (IPC_initialize(ROLE_MISSION) != 0) {
        printf("[OBC_MISSION] Failed to init IPC.\n");
        fflush(stdout);
        return 1;
    }

    if (mission_health_init() != 0) {
        fprintf(stderr, "[OBC MISSION] Failed to initialize health tracking.\n");
        return 1;
    }

    if (init_scheduler_thread() != 0 || init_autonomy_thread() != 0) {
        fprintf(stderr, "[OBC MISSION] Failed to start a worker thread.\n");
        return 1;
    }

    if (heartbeat_thread_init() != 0) {
        fprintf(stderr, "[OBC MISSION] Failed to start heartbeat thread.\n");
        return 1;
    }

    for (;;) {
        sleep(1);
    }

    return 0;
}
