#include <stdio.h>
#include <unistd.h>
#include "storage.h"
#include "filesystem.c"
#include "heartbeat.h"

int main(void) {
    printf("[OBC DATA] Initializing.\n");
    fflush(stdout);

    if (IPC_initialize(ROLE_DATA) != 0) {
        printf("[OBC DATA] Failed to init IPC.\n");
        return 1;
    }

    storage_thread_init();

    for (;;) { sleep(1); }

    return 0;
}
