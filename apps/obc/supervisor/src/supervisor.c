#include "supervisor.h"

#include <stdio.h>
#include <spawn.h>
#include <string.h>

#include "pthread.h"

extern char **environ;

typedef struct {
    const char *name;
    const char *path;
    pid_t pid;
} obc_process_t;

static obc_process_t processes[] = {
    { "fdir", "./obc_fdir", -1 },
    { "commands", "./obc_commands", -1 },
    { "compute", "./obc_compute", -1},
    { "data", "./obc_data", -1 },
    { "mission", "./obc_mission", -1},
    { "time", "./obc_time", -1},
};

#define NUM_PROCESSES (sizeof(processes) / sizeof(processes[0]))

/* Restarts the actual programs/processes */

/*
Also needs an init for getting all other processes up and running
*/
int init_supervisor(void)
{
    /* Start all other processes */
}

int supervisor_start_all(void)
{
    for (size_t i = 0; i < NUM_PROCESSES; i++) {
        char *argv[] = { (char *)processes[i].path, NULL };
        int rc = posix_spawn(&processes[i].pid, processes[i].path,
            NULL, NULL, argv, environ);
        
        if (rc != 0) {
            fprintf(stderr, "supervisor: failed to spawn %s: %s\n", processes[i].name, strerror(rc));
            return -1;
        }
    }
    return 0;
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