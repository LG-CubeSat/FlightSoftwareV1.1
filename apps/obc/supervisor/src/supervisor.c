#include "supervisor.h"

#include <stdio.h>
#include <spawn.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include "processes.h"
#define PERIODIC_HEARTBEAT_SEC 1
/* Restarts the actual programs/processes */

/*
Also needs an init for getting all other processes up and running
*/
int init_supervisor(void)
{
    printf("[SUPERVISOR] Initializing.\n");
    /* Start all other processes */
    printf("[SUPERVISOR] Attmepting to start all processes.\n");
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
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }

    return NULL;
}

/*
Reactive Thread for FDIR requests to shutdown
*/

int init_shutdown_thread(void)
{
    printf("[SUPERVISOR SHUTDOWN] Attempting Init.\n");

    pthread_t shutdown_pthread;
    int ret = pthread_create(&shutdown_pthread, NULL, shutdown_thread, NULL);

    if (ret != 0) {
        fprintf(stderr, "[SUPERVISOR SHUTDOWN] failed to initialize thread: %d\n", ret);
    } else {
        printf("[SUPERVISOR SHUTDOWN] Successfully Initialized.\n");
    }
    return ret;
}

void *shutdown_thread(void *arg)
{
    (void)arg;

    printf("[SHUTDOWN THREAD] Running now");

    for(;;) {

    }

    return NULL;
}
