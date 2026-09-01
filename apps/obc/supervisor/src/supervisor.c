#include "supervisor.h"

#include <stdio.h>
#include "pthread.h"

/* Restarts the actual programs/processes */

/*
Also needs an init for getting all other processes up and running
*/
int init_supervisor(void)
{
    /* Start all other processes */
}

/*
Periodic Thread for heartbeat (actual data collection)
*/
int init_hearbeat(void)
{
    printf("[SUPERVISOR HEARTBEAT] Attempting Initialization.\n");
    pthread_t heartbeat_pthread;
    int ret = pthread_create(heartbeat_pthread, NULL, heartbeat, NULL);

    if (ret == NULL) {
        fprintf(stderr, "[SUPERVISOR HEARTBEAT] Thread failed to create: %d\n", ret);
    } else {
        printf("[SUPERVISOR HEARTBEAT] Init Successful.\n");
    }
    return ret;
}

void heartbeat(void)
{
    
}

/*
Reactive Thread for FDIR requests to shutdown
*/

int init_shutdown(void)
{
    printf("[SUPERVISOR SHUTDOWN] Attempting Init.\n");

    pthread_t shutdown_pthread;
    int ret = pthread_create(shutdown_pthread, NULL, shutdown, NULL);

    if (ret == NULL) {
        fprintf(stderr, "[SUPERVISOR SHUTDOWN] failed to initialize thread: %d\n", ret);
    } else {
        printf("[SUPERVISOR SHUTDOWN] Successfully Initialized.\n");
    }
    return ret;
}

void shutdown(void)
{
    
}