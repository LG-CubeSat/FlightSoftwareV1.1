#include "stdio.h"
#include "obc_ipc.h"
#include "unistd.h"
#include "scheduler.h"
#include "heartbeat.h"

int main(void) {
    printf("[OBC MISSION] Initializing.\n");
    fflush(stdout);

    if (IPC_initialize(ROLE_MISSION) != 0) {
        printf("[OBC_MISSION] Failed to init IPC.\n");
        fflush(stdout);
        return 1;
    }

    init_scheduler_thread();
    heartbeat_thread_init();

    for (;;) {
        sleep(1);
    }

    return 0;
}