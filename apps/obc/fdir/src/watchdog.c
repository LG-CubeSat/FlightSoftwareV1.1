#include "watchdog.h"

#include "stdio.h"
#include "time.h"
#include "pthread.h"
#include "obc_ipc.h"
#include "obc_sleep_until.h"

/* Makes sure the programs aren't frozen or anything by interperating the data of supervisor */

#define WATCHDOG_PERIOD_SEC 1
/*
Watch Dog thread (periodic)
*/

int watchdog_thread_init(void)
{
    printf("[WATCHDOG] Attempting Thread Init.\n");
    pthread_t watchdog_pthread;

    int ret = pthread_create(&watchdog_pthread, NULL, watchdog_thread, NULL);
    if (ret != 0) {
        fprintf(stderr, "[WATCHDOG] Thread failed to create: %d\n", ret);
    } else {
        printf("[WATCHDOG] successfully created pthread.\n");
    }
    return ret;
}

void *watchdog_thread(void *arg)
{
    (void)arg;

    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);

    for (;;) {
        IPC_send(ROLE_SUPERVISOR, NULL, 0); // zero payload ping

        next.tv_sec += WATCHDOG_PERIOD_SEC;
        obc_sleep_until(&next);
    }
}