#include "stdio.h"
#include "obc_ipc.h"
#include "unistd.h"
#include "scheduler.h"

int main(void) {
    printf("[OBC MISSION] Initializing.\n");
    
    if (IPC_initialize(ROLE_MISSION) != 0) {
        printf("[OBC_MISSION] Failed to init IPC.\n");
        return 1;
    }

    init_scheduler_thread();

    for (;;) {
        sleep(1);
    }

    return 0;
}