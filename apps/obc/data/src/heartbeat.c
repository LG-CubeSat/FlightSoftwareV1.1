#include "heartbeat.h"

#include <stdio.h>
#include <time.h>
#include "pthread.h"
#include "obc_ipc.h"
#include "obc_sleep_until.h"

/* Proves this process is alive to supervisor's frozen-check -- without
   this, supervisor has no way to tell "still running" from "hung", and
   will restart a perfectly healthy process after HEARTBEAT_TIMEOUT_SEC. */

#define HEARTBEAT_PERIOD_SEC 1

int heartbeat_thread_init(void)
{
    printf("[HEARTBEAT] Attempting Thread Init.\n");
    fflush(stdout);
    pthread_t heartbeat_pthread;

    int ret = pthread_create(&heartbeat_pthread, NULL, heartbeat_thread, NULL);
    if (ret != 0) {
        fprintf(stderr, "[HEARTBEAT] Thread failed to create: %d\n", ret);
    } else {
        printf("[HEARTBEAT] successfully created pthread.\n");
        fflush(stdout);
    }
    return ret;
}

void *heartbeat_thread(void *arg)
{
    (void)arg;

    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);

    for (;;) {
        IPC_send(ROLE_SUPERVISOR, NULL, 0); // zero payload ping

        next.tv_sec += HEARTBEAT_PERIOD_SEC;
        obc_sleep_until(&next);
    }
}
