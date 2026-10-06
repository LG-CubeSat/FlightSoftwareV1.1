#include <stdio.h>
#include <unistd.h>
#include "storage.h"
#include "filesystem.h"
#include "heartbeat.h"
#include "data_health.h"
#include "obc_log.h"

int main(void) {

    if (obc_log_init("data") != 0) {
        fprintf(stderr, "[OBC DATA] Failed to initialize logging.\n");
    }

    LOG_INFO("Initializing");


    printf("[OBC DATA] Initializing.\n");
    fflush(stdout);

    if (IPC_initialize(ROLE_DATA) != 0) {
        printf("[OBC DATA] Failed to init IPC.\n");
        return 1;
    }

    if (data_health_init() != 0) {
        fprintf(stderr, "[OBC DATA] Failed to initialize health tracking.\n");
        return 1;
    }

    if (storage_thread_init() != 0) {
        fprintf(stderr, "[OBC DATA] Failed to start storage thread.\n");
        return 1;
    }

    if (heartbeat_thread_init() != 0) {
        fprintf(stderr, "[OBC DATA] Failed to start heartbeat thread.\n");
        return 1;
    }

    for (;;) { sleep(1); }

    return 0;
}
