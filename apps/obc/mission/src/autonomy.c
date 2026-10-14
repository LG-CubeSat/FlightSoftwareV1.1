#include "autonomy.h"

#include <stdio.h>
#include <stdlib.h>
#include "time.h"
#include "pthread.h"

#include "payload_commander.h"
#include "obc_sleep_until.h"
#include "mission_health.h"

#define DEFAULT_AUTONOMY_INTERVAL_SEC 600
#define AUTONOMY_INTERVAL_ENV "MISSION_AUTONOMY_INTERVAL_SEC"

typedef struct {
    const char *name;
    int interval_sec;
    struct timespec last_fired;
    int (*action)(void);
} autonomy_action_t;

static autonomy_action_t actions[] = {
    {
        "point ADCS to sun",
        DEFAULT_AUTONOMY_INTERVAL_SEC,
        {0},
        payload_commander_point_to_sun
    },
    // new commands here
};

static int get_autonomy_interval_sec(void)
{
    const char *text = getenv(AUTONOMY_INTERVAL_ENV);
    if (text == NULL || text[0] == '\0') {
        return DEFAULT_AUTONOMY_INTERVAL_SEC;
    }

    int interval = atoi(text);
    return interval > 0 ? interval : DEFAULT_AUTONOMY_INTERVAL_SEC;
}

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

    int interval_sec = get_autonomy_interval_sec();
    for (size_t i = 0; i < sizeof(actions) / sizeof(actions[0]); i++) {
        actions[i].interval_sec = interval_sec;
    }

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
                /* Only mark this as done if it actually went out -- a
                   startup race (e.g. commands isn't listening yet) would
                   otherwise get treated the same as a real send and go
                   unretried for a full interval instead of trying again
                   on the very next tick. */
                if (actions[i].action() == 0) {
                    actions[i].last_fired = now;
                }
            }
        }

        mission_health_autonomy_progress();

        next.tv_sec += 1;
        obc_sleep_until(&next);
    }

    return NULL;
}
