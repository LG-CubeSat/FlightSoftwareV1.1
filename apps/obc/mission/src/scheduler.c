#include "scheduler.h"

#include <stdio.h>
#include <time.h>
#include "obc_sleep_until.h"
#include "payload_commander.h"
#include "pthread.h"

#define ASCENT_WAIT_SEC 5400 // ~90 min: typical HAB ascent to burst altitude at ~5 m/s; tune once real ascent rate/fill is known
#define PHOTO_PATH "/tmp/photos" // placeholder

typedef enum {
    MISSION_WAITING_FOR_ASCENT,
    MISSION_TAKING_PHOTO,
    MISSION_DOWNLINKING,
    MISSION_DONE
} mission_state_t;

mission_state_t current_state = MISSION_WAITING_FOR_ASCENT;

int init_scheduler_thread(void) {
    printf("[MISSION SCHEDULER] Attempting to create pthread.\n");
    fflush(stdout);
    pthread_t scheduler_pthread;
    int ret = pthread_create(&scheduler_pthread, NULL, scheduler_thread, NULL);
    if (ret != 0) {
        printf("[MISSION SCHEDULER] Failed to create pthread\n");
    } else {
        printf("[MISSION SCHEDULER] Successfully created pthread.\n");
    }
    fflush(stdout);
    return ret;
}

void *scheduler_thread(void *arg) {
    (void)arg;
    struct timespec start, next;
    clock_gettime(CLOCK_MONOTONIC, &start);
    next = start;

    for (;;) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double elapsed = (now.tv_sec - start.tv_sec) + (now.tv_nsec - start.tv_nsec) / 1e9;

        switch (current_state) {
            case MISSION_WAITING_FOR_ASCENT:
                if (elapsed >= ASCENT_WAIT_SEC) {
                    printf("[SCHEDULER] Ascent window reached\n");
                    fflush(stdout);
                    current_state = MISSION_TAKING_PHOTO;
                }
                break;
            case MISSION_TAKING_PHOTO:
                payload_commander_take_photo(PHOTO_PATH);
                current_state = MISSION_DOWNLINKING;
                break;
            case MISSION_DOWNLINKING:
                payload_commander_downlink_photo(PHOTO_PATH);
                current_state = MISSION_DONE;
                break;
            case MISSION_DONE:
                break;
        }

        next.tv_sec += 1;
        obc_sleep_until(&next);
    }
}
