#include "scheduler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include "obc_sleep_until.h"
#include "payload_commander.h"
#include "pthread.h"

#define ASCENT_WAIT_SEC 5400 // ~90 min: typical HAB ascent to burst altitude at ~5 m/s; tune once real ascent rate/fill is known
#define PHOTO_PATH "/tmp/photos" // placeholder
#define COMPRESSED_PHOTO_PATH "/tmp/photos.rice"
#define DEFAULT_STATE_PATH "/var/lib/obc/mission_state"
#define STEP_RETRY_LIMIT 3 // attempts per step before giving up on the cycle
#define COOLDOWN_SEC 600 // 10 min between cycles, need to tune

typedef enum {
    MISSION_WAITING_FOR_ASCENT,
    MISSION_TAKING_PHOTO,
    MISSION_COMPRESSING,
    MISSION_DOWNLINKING,
    MISSION_COOLDOWN,
} mission_state_t;

mission_state_t current_state = MISSION_WAITING_FOR_ASCENT;

/* Wall clock, not CLOCK_MONOTONIC: this has to survive a process restart and
   a Pi reboot, and monotonic resets to zero on boot. Loop pacing below still
   uses monotonic -- different job, different clock. */
static time_t mission_start_unix = 0;
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

static const char *get_state_path(void)
{
    const char *env = getenv("MISSION_STATE_PATH");
    return (env != NULL && env[0] != '\0') ? env : DEFAULT_STATE_PATH;
}

/* Deliberate exception to roles.md's "all filesystem access goes through
   data": this is crash-recovery state, and routing it through another
   process would mean mission can't recover while data is itself restarting.
   Only mission ever touches this file, so the race that rule guards against
   doesn't apply. Written temp-then-rename so a power loss mid-write can
   never leave a torn file -- rename(2) is atomic. */
static void mission_state_save(void)
{
    const char *path = get_state_path();
    char tmp_path[PATH_MAX];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    FILE *f = fopen(tmp_path, "w");
    if (f == NULL) {
        fprintf(stderr, "[SCHEDULER] cannot write %s: %s\n", tmp_path, strerror(errno));
        return;
    }

    fprintf(f, "%d %lld\n", (int)current_state, (long long)mission_start_unix);
    fflush(f);        // libc buffer -> kernel
    fsync(fileno(f)); // kernel -> actual storage
    fclose(f);

    if (rename(tmp_path, path) != 0) {
        fprintf(stderr, "[SCHEDULER] cannot rename %s -> %s: %s\n", tmp_path, path, strerror(errno));
    }
}

/* A state file can be truncated or corrupted by a power loss, so treat it as
   untrusted input and range-check it before believing it. */
static void mission_state_load(void)
{
    const char *path = get_state_path();
    FILE *f = fopen(path, "r");

    if (f != NULL) {
        int phase = 0;
        long long saved_start = 0;
        int parsed = fscanf(f, "%d %lld", &phase, &saved_start);
        fclose(f);

        if (parsed == 2 &&
            phase >= MISSION_WAITING_FOR_ASCENT && phase <= MISSION_COOLDOWN &&
            saved_start > 0) {
            current_state = (mission_state_t)phase;
            mission_start_unix = (time_t)saved_start;

            /* cooldown_start is monotonic and deliberately not persisted --
               resume into a fresh cooldown rather than firing instantly. */
            if (current_state == MISSION_COOLDOWN) {
                clock_gettime(CLOCK_MONOTONIC, &cooldown_start);
            }

            printf("[SCHEDULER] resumed mission: phase=%d, started %lld\n", phase, saved_start);
            fflush(stdout);
            return;
        }
        fprintf(stderr, "[SCHEDULER] state file %s unreadable/corrupt, starting fresh\n", path);
    }

    mission_start_unix = time(NULL);
    current_state = MISSION_WAITING_FOR_ASCENT;
    printf("[SCHEDULER] fresh mission, start=%lld\n", (long long)mission_start_unix);
    fflush(stdout);
    mission_state_save();
}

/* Every state transition goes through here so persisting can't be forgotten. */
static void set_state(mission_state_t next)
{
    current_state = next;
    retry_count = 0;
    mission_state_save();
}

static void enter_cooldown(void)
{
    clock_gettime(CLOCK_MONOTONIC, &cooldown_start);
    set_state(MISSION_COOLDOWN);
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
    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);

    int ascent_wait_sec = get_ascent_wait_sec();

    mission_state_load(); // restores phase + mission_start_unix

    for (;;) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now); // monotonic: cooldown pacing
        double elapsed = difftime(time(NULL), mission_start_unix); // wall clock: survives restarts

        switch (current_state) {
            case MISSION_WAITING_FOR_ASCENT:
                if (elapsed >= ascent_wait_sec) {
                    printf("[SCHEDULER] Ascent window reached\n");
                    fflush(stdout);
                    set_state(MISSION_TAKING_PHOTO);
                }
                break;
            case MISSION_TAKING_PHOTO:
                if (payload_commander_take_photo(PHOTO_PATH) == 0) {
                    set_state(MISSION_COMPRESSING);
                } else if (++retry_count >= STEP_RETRY_LIMIT) {
                    fprintf(stderr, "[SCHEDULER] Photo capture failed %d times, abandoning this cycle\n", retry_count);
                    enter_cooldown();
                }
                break;
            case MISSION_COMPRESSING:
                if (payload_commander_compress_photo(PHOTO_PATH, COMPRESSED_PHOTO_PATH) == 0) {
                    set_state(MISSION_DOWNLINKING);
                } else if (++retry_count >= STEP_RETRY_LIMIT) {
                    fprintf(stderr, "[SCHEDULER] Photo compression failed %d times, abandoning this cycle\n", retry_count);
                    enter_cooldown();
                }
                break;
            case MISSION_DOWNLINKING:
                if (payload_commander_downlink_photo(COMPRESSED_PHOTO_PATH) == 0) {
                    enter_cooldown(); // success also goes to cooldown. Next cycle starts after the pause
                } else if (++retry_count >= STEP_RETRY_LIMIT) {
                    fprintf(stderr, "[SCHEDULER] Downlink failed %d times, abandoning this cycle\n", retry_count);
                    enter_cooldown();
                }
                break;
            case MISSION_COOLDOWN: { // braces keep cooldown_elapsed's scope local to this case
                double cooldown_elapsed = (now.tv_sec - cooldown_start.tv_sec) + (now.tv_nsec - cooldown_start.tv_nsec) / 1e9;
                if (cooldown_elapsed >= COOLDOWN_SEC) {
                    printf("[SCHEDULER] cooldown complete, starting next capture cycle\n");
                    fflush(stdout);
                    set_state(MISSION_TAKING_PHOTO);
                }
                break;
            }
        }

        next.tv_sec += 1;
        obc_sleep_until(&next);
    }
}
