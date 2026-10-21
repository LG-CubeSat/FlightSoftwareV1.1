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
#include "mission_health.h"
#include <inttypes.h>

#define ASCENT_WAIT_SEC 5400 // ~90 min: typical HAB ascent to burst altitude at ~5 m/s; tune once real ascent rate/fill is known
#define PHOTO_PATH "/tmp/photos" // placeholder
#define COMPRESSED_PHOTO_PATH "/tmp/photos.rice"
#define DEFAULT_STATE_PATH "/var/lib/obc/mission_state"
#define STEP_RETRY_LIMIT 3 // attempts per step before giving up on the cycle
#define COOLDOWN_SEC 600 // 10 min between cycles, need to tune
#define TELEMETRY_BATCH_LIMIT 8U
#define TELEMETRY_DOWNLINK_INTERVAL_SEC 30

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
/*
Byte offset of the first telemetry record that has not yet been successfully downlinked.
*/
static uint64_t telemetry_cursor = 0;

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

static int get_telemetry_downlink_interval_sec(void)
{
    const char *env = getenv("MISSION_TELEMETRY_DOWNLINK_INTERVAL_SEC");

    if (env == NULL) {
        return TELEMETRY_DOWNLINK_INTERVAL_SEC;
    }

    int value = atoi(env);
    return value > 0 ? value : TELEMETRY_DOWNLINK_INTERVAL_SEC;
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

    fprintf(f, "%d %lld %" PRIu64 "\n", (int)current_state, (long long)mission_start_unix, telemetry_cursor);
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
        uint64_t saved_cursor = 0;
        
        int parsed = fscanf(f, "%d %lld %" SCNu64, &phase, &saved_start, &saved_cursor);
        fclose(f);

        if ((parsed == 2 || parsed == 3) &&
            phase >= MISSION_WAITING_FOR_ASCENT && phase <= MISSION_COOLDOWN &&
            saved_start > 0) {
            current_state = (mission_state_t)phase;
            mission_start_unix = (time_t)saved_start;
            telemetry_cursor = parsed == 3 ? saved_cursor : 0;

            /* cooldown_start is monotonic and deliberately not persisted --
               resume into a fresh cooldown rather than firing instantly. */
            if (current_state == MISSION_COOLDOWN) {
                clock_gettime(CLOCK_MONOTONIC, &cooldown_start);
            }

            printf("[SCHEDULER] resumed mission: phase=%d, started %lld, telemetry_cursor=%" PRIu64 "\n", phase, saved_start, telemetry_cursor);
            fflush(stdout);
            return;
        }
        fprintf(stderr, "[SCHEDULER] state file %s unreadable/corrupt, starting fresh\n", path);
    }

    mission_start_unix = time(NULL);
    current_state = MISSION_WAITING_FOR_ASCENT;
    telemetry_cursor = 0;
    printf("[SCHEDULER] fresh mission, start=%lld, telemetry_cursor=%" PRIu64 "\n", (long long)mission_start_unix, telemetry_cursor);
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

static void run_telemetry_downlink_batch(void)
{
    uint64_t cursor_before_batch = telemetry_cursor;
    size_t records_sent = 0;
    int end_of_log = 0;

    mission_health_payload_begin();

    int result = payload_commander_downlink_telemetry_batch(
        &telemetry_cursor,
        TELEMETRY_BATCH_LIMIT,
        &records_sent,
        &end_of_log
    );

    mission_health_payload_end();

    /*
    Save any successful transmitted prefix even if a later record
    in this batch failed
    */
    if (telemetry_cursor != cursor_before_batch) {
        mission_state_save();
    }

    if (result != 0) {
        fprintf(
            stderr,
            "[SCHEDULER] telemetry batch failed after %zu records; cursor=%" PRIu64 "\n",
            records_sent,
            telemetry_cursor
        );
        return;
    }

    if (records_sent > 0) {
        printf(
            "[SCHEDULER] downlinked %zu telemetry records; cursor=%" PRIu64 ", end_of_log=%d\n",
            records_sent, telemetry_cursor, end_of_log
        );
        fflush(stdout);
    }
}

void *scheduler_thread(void *arg) {
    (void)arg;
    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);

    int ascent_wait_sec = get_ascent_wait_sec();

    mission_state_load(); // restores phase + mission_start_unix + telem cursor
    mission_health_scheduler_progress();

    int telemetry_downlink_interval_sec =
        get_telemetry_downlink_interval_sec();
    
    struct timespec last_telemetry_attempt = {0};

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
            case MISSION_TAKING_PHOTO: {
                mission_health_payload_begin();
                int result = payload_commander_take_photo(PHOTO_PATH);
                mission_health_payload_end();
                if (result == 0) {
                    set_state(MISSION_COMPRESSING);
                } else if (++retry_count >= STEP_RETRY_LIMIT) {
                    fprintf(stderr, "[SCHEDULER] Photo capture failed %d times, abandoning this cycle\n", retry_count);
                    enter_cooldown();
                }
                break;
            }
            case MISSION_COMPRESSING: {
                mission_health_payload_begin();
                int result = payload_commander_compress_photo(PHOTO_PATH, COMPRESSED_PHOTO_PATH);
                mission_health_payload_end();
                if (result == 0) {
                    set_state(MISSION_DOWNLINKING);
                } else if (++retry_count >= STEP_RETRY_LIMIT) {
                    fprintf(stderr, "[SCHEDULER] Photo compression failed %d times, abandoning this cycle\n", retry_count);
                    enter_cooldown();
                }
                break;
            }
            case MISSION_DOWNLINKING: {
                mission_health_payload_begin();
                int result = payload_commander_downlink_photo(COMPRESSED_PHOTO_PATH);
                mission_health_payload_end();
                if (result == 0) {
                    enter_cooldown(); // success also goes to cooldown. Next cycle starts after the pause
                } else if (++retry_count >= STEP_RETRY_LIMIT) {
                    fprintf(stderr, "[SCHEDULER] Downlink failed %d times, abandoning this cycle\n", retry_count);
                    enter_cooldown();
                }
                break;
            }
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

        struct timespec telemetry_now;
        clock_gettime(CLOCK_MONOTONIC, &telemetry_now);

        double since_telemetry_attempt = (telemetry_now.tv_sec - last_telemetry_attempt.tv_sec) +
            (telemetry_now.tv_nsec - last_telemetry_attempt.tv_nsec) / 1e9;

        if (
            since_telemetry_attempt >= telemetry_downlink_interval_sec
        ) {
            run_telemetry_downlink_batch();

            /*
            Re-read the clock after the operation. A failed Data request may have consumed its complete timeout.
            */
            clock_gettime(
                CLOCK_MONOTONIC,
                &last_telemetry_attempt
            );
        }

        /* Reaching here proves the complete state handler returned. */
        mission_health_scheduler_progress();

        next.tv_sec += 1;
        obc_sleep_until(&next);
    }
}
