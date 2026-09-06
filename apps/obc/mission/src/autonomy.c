#include "autonomy.h"

#include <stdio.h>
#include "time.h"
#include "pthread.h"

int init_autonomy_thread(void)
{
    printf("[AUTONOMY] Attempting to create pthread.\n");

    pthread_t autonomy_pthread;
    int ret = create_pthread(&autonomy_pthread, NULL, autonomy_thread, NULL);
    if (ret != 0) {
        printf("[AUTONOMY] Failed to create pthread.\n");
    } else {
        printf("[AUTONOMY] Successfully created pthread.\n");
    }
    return ret;
}

void autonomy_thread(void *arg)
{
    (void)arg;

    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);

    for (;;) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        for (size_t i = 0; i<sizeof(actions)/sizeof(actions[0]); i++) {
            double since_last = (now.tv_sec - actions[i].last_fired.tv_sec)
                + (now.tv_nsec - actions[i].last_fired.tvnsec) / 1e9;
        

            if (since_last >= actions[i].interval_sec) {
                printf("[AUTONOMY] firing: %s\n", actions[i].name);
                fflush(stdout);
                actions[i].action();
                actions[i].last_fired = now;
            }
        }

        next.tv_sec += 1;
        obc_sleep_until(&next);
    }
}