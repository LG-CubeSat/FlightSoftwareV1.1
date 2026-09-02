#ifndef PROCESSES_H
#define PROCESSES_H

#include <spawn.h>
#include "obc_ipc.h"

extern char **environ;

typedef struct {
    const char *name;
    const char *path;
    pid_t pid;
    OBC_Roles_t role;
} obc_process_t;

typedef enum {
    FDIR = 0,
    COMMANDS = 1,
    COMPUTE = 2,
    DATA = 3,
    MISSION = 4,
    TIME = 5,
} OBC_Process_Indice;

int start_all_processes(void);

void supervisor_heartbeat(void);

int supervisor_shutdown_process(obc_process_t *proc);
int supervisor_restart_process(obc_process_t *proc);
obc_process_t *supervisor_find_process(OBC_Roles_t role);

#endif // PROCESSES_H