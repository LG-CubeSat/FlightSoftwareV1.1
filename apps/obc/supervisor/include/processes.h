#ifndef PROCESSES_H
#define PROCESSES_H

#include <spawn.h>

extern char **environ;

typedef struct {
    const char *name;
    const char *path;
    pid_t pid;
} obc_process_t;

int start_all_processes(void);

#endif PROCESSES_H