#include "scheduler.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "obc_sleep_until.h"
#include "payload_commander.h"
#include "pthread.h"

#define ASCENT_WAIT_SEC 5400 // ~90 min: typical HAB ascent to burst altitude at ~5 m/s; tune once real ascent rate/fill is known
#define PHOTO_PATH "/tmp/photos" // placeholder
#define COMPRESSED_PHOTO_PATH "/tmp/photos.rice"
#define STEP_RETRY_LIMIT 3 // attempts per step before giving up on the cycle
#define COOLDOWN_SEC 600 // 10 min between cycles, need to tune

static int retry_count = 0; // reused across the steps for whichever one is active
static struct timespec cooldown_start; // set when we enter MISSION_COOLDOWN

/* Overridable so an integration test can run the full timeline in seconds
   instead of waiting out the real ~90 minute ascent. Production default is
   untouched unless the env var is set. */
static int get_ascent_wait_sec(void)
{
    const char *env = getenv("MISSION_ASCENT_WAIT_SEC");
    if (env == NULL) return ASCENT_WAIT_SEC;

    int val = atoi(env);
    return (val > 0) ? val : ASCENT_WAIT_SEC;
}

typedef enum {
    MISSION_WAITING_FOR_ASCENT,
    MISSION_TAKING_PHOTO,
    MISSION_COMPRESSING,
    MISSION_DOWNLINKING,
    MISSION_COOLDOWN,
} mission_state_t;

mission_state_t current_state = MISSION_WAITING_FOR_ASCENT;

static void enter_cooldown(void)
{
    clock_gettime(CLOCK_MONOTONIC, &cooldown_start);
    current_state = MISSION_COOLDOWN;
    retry_count = 0;
}

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

    int ascent_wait_sec = get_ascent_wait_sec();

    for (;;) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double elapsed = (now.tv_sec - start.tv_sec) + (now.tv_nsec - start.tv_nsec) / 1e9;

        switch (current_state) {
            case MISSION_WAITING_FOR_ASCENT:
                if (elapsed >= ascent_wait_sec) {
                    printf("[SCHEDULER] Ascent window reached\n");
                    fflush(stdout);
                    current_state = MISSION_TAKING_PHOTO;
                }
                break;
            case MISSION_TAKING_PHOTO:
                if (payload_commander_take_photo(PHOTO_PATH) == 0) {
                    current_state = MISSION_COMPRESSING;
                    retry_count = 0; // reset for next step    
                } else if (++retry_count >= STEP_RETRY_LIMIT) {
                    fprintf(stderr, "[SCHEDULER] Photo capture failed %d times, abandoning this cycle\n", retry_count);
                    enter_cooldown();
                }
                break;
            case MISSION_COMPRESSING:
                if (payload_commander_compress_photo(PHOTO_PATH, COMPRESSED_PHOTO_PATH) == 0) {
                    current_state = MISSION_DOWNLINKING;
                    retry_count = 0; // reset for next step
                } else if (++retry_count >= STEP_RETRY_LIMIT) {
                    fprintf(stderr, "[SCHEDULER] Photo Compression failed %d times, abandoning this cycle\n", retry_count);
                    enter_cooldown();
                }
                break;
            case MISSION_DOWNLINKING:
                if (payload_commander_downlink_photo(COMPRESSED_PHOTO_PATH) == 0) {
                    enter_cooldown(); // success also goes to cooldown. Next cycle starts after the pause
                } else if (++retry_count >= STEP_RETRY_LIMIT) {
                    fprintf(stderr, "[SCHEDULER] downlink failed %d times, abandoning this cylce\n", retry_count);
                    enter_cooldown();
                }
                break;
            case MISSION_COOLDOWN: { // put brackers to keep scope of cooldown_elapsed local
                // basically just waits and restarts back at mission taking photo.
                double cooldown_elapsed = (now.tv_sec - cooldown_start.tv_sec) + (now.tv_nsec - cooldown_start.tv_nsec) / 1e9;
                if (cooldown_elapsed >= COOLDOWN_SEC) {
                    printf("[SCHEDULER] cooldown complete, starting next capture cycle\n");
                    fflush(stdout);
                    current_state = MISSION_TAKING_PHOTO;
                    retry_count = 0;
                }
                break;
            }
        }

        next.tv_sec += 1;
        obc_sleep_until(&next);
    }
}
