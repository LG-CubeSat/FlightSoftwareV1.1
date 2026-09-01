#include "processes.h"

#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <libgen.h>
#include <stdio.h>

#define NUM_PROCESSES (sizeof(processes) / sizeof(processes[0]))

static obc_process_t processes[] = {
    { "fdir", "./obc_fdir", -1 },
    { "commands", "./obc_commands", -1 },
    { "compute", "./obc_compute", -1},
    { "data", "./obc_data", -1 },
    { "mission", "./obc_mission", -1},
    { "time", "./obc_time", -1},
};

/* Starts the processes of the OBC */
int start_all_processes(void)
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

