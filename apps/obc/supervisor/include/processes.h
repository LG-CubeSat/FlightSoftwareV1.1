#ifndef PROCESSES_H
#define PROCESSES_H

#include <spawn.h>
#include <stdint.h>
#include <limits.h>
#include "obc_ipc.h"
#include "obc_supervisor_protocol.h"
#include <pthread.h>

extern char **environ;

typedef struct {
    const char *name;
    const char *exe_name;             // binary name only, e.g. "obc_fdir"
    char resolved_path[PATH_MAX];     // filled by supervisor_resolve_paths()
    pid_t pid;
    OBC_Roles_t role;
    int restart_count; // consecutive crash-restarts since it last ran cleanly
    
    // serialized kill/wait/spawn operations for this process. It is seperate from proc_lock which only protects access to the pid
    pthread_mutex_t lifecycle_lock;
} obc_process_t;

int supervisor_resolve_paths(void);
int start_all_processes(void);

void supervisor_heartbeat(void);

int supervisor_shutdown_process(obc_process_t *proc);
int supervisor_restart_process(obc_process_t *proc);
obc_process_t *supervisor_find_process(OBC_Roles_t role);
void supervisor_shutdown_all(void);
void supervisor_mark_alive(OBC_Roles_t role);

#endif // PROCESSES_H