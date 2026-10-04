#include "watchdog.h"

#include "stdio.h"
#include "time.h"
#include "pthread.h"

#include "fallback.h"
#include "health_monitor.h"
#include "obc_ipc.h"
#include "obc_sleep_until.h"

/* Makes sure the programs aren't frozen or anything by interperating the data of supervisor */

#define WATCHDOG_PERIOD_SEC 1
#define ROLE_HEARTBEAT_TIMEOUT_SEC 5

/*
FDIR monitors every OBC role except itself and supervisor.
Supervisor is the parent process and is handled seperately by and eventual hardware level watchdog...
*/
static const OBC_Roles_t monitored_roles[] = {
    ROLE_COMMANDS,
    ROLE_COMPUTE,
    ROLE_DATA,
    ROLE_MISSION,
    ROLE_TIME,
};

/*
Once entry per role once a fault has been successfully reported,
supress additonal restart requests until that role heartbeats again
*/
static int fault_latched[ROLE_COUNT];

static const char *role_name(OBC_Roles_t role) {
    switch (role) {
        case ROLE_COMMANDS: return "commands";
        case ROLE_COMPUTE: return "compute";
        case ROLE_DATA: return "data";
        case ROLE_MISSION: return "mission";
        case ROLE_TIME: return "time";
        default: return "unknown";
    }
}

static double elapsed_seconds(
    const struct timespec *start,
    const struct timespec *end
)
{
    return (double)(end->tv_sec - start->tv_sec) + (double)(end->tv_nsec - start->tv_nsec) / 1.0e9;
}

/*
A role is stale when:
1. It previously checked in but its last hearbeat is too old
2. It never checked in and FDIR's startup grace period expired
*/
static int role_is_stale(
    OBC_Roles_t role,
    const struct timespec *watchdog_started,
    const struct timespec *now
)
{
    struct timespec last_seen;

    if (!health_monitor_get_last_heartbeat(role, &last_seen)) {
        return elapsed_seconds(watchdog_started, now) > ROLE_HEARTBEAT_TIMEOUT_SEC;
    }

    return elapsed_seconds(&last_seen, now) > ROLE_HEARTBEAT_TIMEOUT_SEC;
}

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
    
    struct timespec watchdog_started;
    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &watchdog_started);
    next = watchdog_started;
    for (;;) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        
        for (size_t i = 0; i < sizeof(monitored_roles) / sizeof(monitored_roles[0]); i++) {
            OBC_Roles_t role = monitored_roles[i];
            int stale = role_is_stale(
                role, &watchdog_started, &now
            );

            if (stale && !fault_latched[role]) {
                fault_report_t report = {
                    .role = role,
                    .fault_type = UNRESPONSIVE,
                };

                fprintf(stderr, "[FDIR WATCHDOG] %s missed its heartbeat deadline\n", role_name(role));

                /*
                Latch only after the request was successfully sent.
                If IPC delivery fails, the next watchdog iteration retries.
                */
                if (fallback_handle_fault(report) == 0) {
                    fault_latched[role] = 1;
                } else {
                    fprintf(
                        stderr,
                        "[FDIR WATCHDOG] recovery request for %s failed. Will retry\n",
                        role_name(role)
                    );
                }
            } else if (!stale && fault_latched[role]) {
                /*
                a new heartbeat proves the restarted/recovered process
                has reached healthy execution again.
                */
               fault_latched[role] = 0;

               printf("[FDIR WATCHDOG] %s heartbeat recovered\n", role_name(role));
               fflush(stdout);
            }
        }
        
        /*
        FDIR still reports its own liveness directly to supervisor.
        */
        IPC_send(ROLE_SUPERVISOR, NULL, 0); // zero payload ping. heartbeat to supervisor

        next.tv_sec += WATCHDOG_PERIOD_SEC;
        obc_sleep_until(&next);
    }
}