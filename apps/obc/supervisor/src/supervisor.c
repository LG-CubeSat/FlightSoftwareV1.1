#include "supervisor.h"

#include <stdio.h>
#include <spawn.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include "processes.h"
#define PERIODIC_HEARTBEAT_SEC 1
/* Restarts the actual programs/processes */

/* Sleeps until the absolute time `next`, without drifting from this
   function's own overhead. clock_nanosleep(TIMER_ABSTIME) does this
   natively on Linux (the real target); macOS's libc has no such call,
   so it's emulated there with a freshly-computed relative delta. */
static void sleep_until(const struct timespec *next)
{
#if defined(__linux__)
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, next, NULL);
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    // notice the extra work since mac has no native absolute sleep. only relative.
    struct timespec delta;
    delta.tv_sec = next->tv_sec - now.tv_sec;
    delta.tv_nsec = next->tv_nsec - now.tv_nsec;
    // corrects for the borrowing of subtraction. think 54 - 17. You get 4 and -3 for each place. So you must do (4-1) and (-3 + 10) = 37.
    if (delta.tv_nsec < 0) {
        delta.tv_sec -= 1;
        delta.tv_nsec += 1000000000L;
    }

    if (delta.tv_sec > 0 || (delta.tv_sec == 0 && delta.tv_nsec > 0)) {
        nanosleep(&delta, NULL);
    }
#endif
}

/*
Also needs an init for getting all other processes up and running
*/
int init_supervisor(void)
{
    printf("[SUPERVISOR] Initializing.\n");
    /* Start all other processes */
    printf("[SUPERVISOR] Attempting to start all processes.\n");
    int ret = start_all_processes();
    if (ret != 0) {
        printf("[SUPERVISOR] Error starting all processes: %d\n", ret);
    } else {
        printf("[SUPERVISOR] Successfully started all processes.");
    }

    /* Add more init logic here */

    return ret;
}

/*
Periodic Thread for heartbeat (actual data collection)
*/
int init_heartbeat_thread(void)
{
    printf("[SUPERVISOR HEARTBEAT] Attempting Initialization.\n");
    pthread_t heartbeat_pthread;
    int ret = pthread_create(&heartbeat_pthread, NULL, heartbeat_thread, NULL);

    if (ret != 0) {
        fprintf(stderr, "[SUPERVISOR HEARTBEAT] Thread failed to create: %d\n", ret);
    } else {
        printf("[SUPERVISOR HEARTBEAT] Init Successful.\n");
    }
    return ret;
}

void *heartbeat_thread(void *arg)
{
    (void)arg;

    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);
    
    for(;;) {

        supervisor_heartbeat();

        next.tv_sec += PERIODIC_HEARTBEAT_SEC;
        sleep_until(&next);
    }

    return NULL;
}

/*
Reactive Thread for FDIR requests to shutdown
*/

int init_shutdown_thread(void)
{
    printf("[SUPERVISOR SHUTDOWN] Attempting Init.\n");

    /* Init IPC for the shutdown task*/

    int ipc_ret = IPC_initialize(ROLE_SUPERVISOR);

    if (ipc_ret != 0) {
        fprintf(stderr, "[SUPERVISOR SHUTDOWN] Failed to init IPC: %d\n", ipc_ret);
    } else {
        printf("[SUPERVISOR SHUTDOWN] IPC Initialized.\n");
    }

    pthread_t shutdown_pthread;
    int pt_ret = pthread_create(&shutdown_pthread, NULL, shutdown_thread, NULL);

    if (pt_ret != 0) {
        fprintf(stderr, "[SUPERVISOR SHUTDOWN] failed to initialize thread: %d\n", pt_ret);
    } else {
        printf("[SUPERVISOR SHUTDOWN] Successfully Initialized.\n");
    }

    return ipc_ret * pt_ret; // OR
}

void *shutdown_thread(void *arg)
{
    (void)arg;

    printf("[SHUTDOWN THREAD] Running now\n");

    for (;;) {
        OBC_Roles_t src;
        supervisor_request_t req;
        int len = IPC_receive(&src, (uint8_t *)&req, sizeof(req));
        if (len != sizeof(req)) {
            continue; // short/malformed message, ignore and keep listening
        }

        obc_process_t *proc = supervisor_find_process((OBC_Roles_t)req.role);
        if (proc == NULL) {
            fprintf(stderr, "[SUPERVISOR SHUTDOWN] unknown role %d requested by role %d\n",
                    req.role, src);
            continue;
        }

        switch (req.cmd) {
        case SUPERVISOR_CMD_SHUTDOWN:
            printf("[SUPERVISOR SHUTDOWN] shutting down %s (requested by role %d)\n",
                   proc->name, src);
            supervisor_shutdown_process(proc);
            break;
        case SUPERVISOR_CMD_RESTART:
            printf("[SUPERVISOR SHUTDOWN] restarting %s (requested by role %d)\n",
                   proc->name, src);
            supervisor_restart_process(proc);
            break;
        default:
            fprintf(stderr, "[SUPERVISOR SHUTDOWN] unknown command %d from role %d\n",
                    req.cmd, src);
            break;
        }
    }

    return NULL;
}
