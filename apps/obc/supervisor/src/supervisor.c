#include "supervisor.h"

#include <stdio.h>
#include <spawn.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include "processes.h"
#include "obc_sleep_until.h"
#define PERIODIC_HEARTBEAT_SEC 1
/* Restarts the actual programs/processes */

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
        printf("[SUPERVISOR] Successfully started all processes.\n");
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

        // TODO: actually do something with the processes that fail heartbeat and frozen check

        next.tv_sec += PERIODIC_HEARTBEAT_SEC;
        obc_sleep_until(&next);
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

    if (ipc_ret != 0 || pt_ret != 0) {
        return -1;
    } else { 
        return 0;
    };
}

void *shutdown_thread(void *arg)
{
    (void)arg;

    printf("[SHUTDOWN THREAD] Running now\n");

    for (;;) {
        OBC_Roles_t src;
        uint8_t buf[sizeof(supervisor_request_t)];
        int len = IPC_receive(&src, buf, sizeof(buf));
        if (len < 0) {
            continue; // receive error, keep listening
        }

        if (len == 0) {
            // zero-payload message: a liveness ping, identity comes from `src` alone
            supervisor_mark_alive(src);
            continue;
        }

        if ((size_t)len != sizeof(supervisor_request_t)) {
            continue; // unrecognized message shape, ignore
        }

        supervisor_request_t req;
        memcpy(&req, buf, sizeof(req));

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
