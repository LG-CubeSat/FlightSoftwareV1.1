#include "autonomy.h"

#include <stdio.h>
#include "time.h"
#include "pthread.h"

#include "payload_commander.h"
#include "obc_sleep_until.h"

typedef struct {
    const char *name;
    int interval_sec;
    struct timespec last_fired;
    int (*action)(void);
} autonomy_action_t;

static autonomy_action_t actions[] = {
    { "point ADCS to sun", 600, {0}, payload_commander_point_to_sun },
    // new commands here
};

int init_autonomy_thread(void)
{
    printf("[AUTONOMY] Attempting to create pthread.\n");

    pthread_t autonomy_pthread;
    int ret = pthread_create(&autonomy_pthread, NULL, autonomy_thread, NULL);
    if (ret != 0) {
        printf("[AUTONOMY] Failed to create pthread.\n");
    } else {
        printf("[AUTONOMY] Successfully created pthread.\n");
    }
    return ret;
}

void *autonomy_thread(void *arg)
{
    (void)arg;

    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);

    for (;;) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        for (size_t i = 0; i<sizeof(actions)/sizeof(actions[0]); i++) {
            double since_last = (now.tv_sec - actions[i].last_fired.tv_sec)
                + (now.tv_nsec - actions[i].last_fired.tv_nsec) / 1e9;
        

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

    return NULL;
}