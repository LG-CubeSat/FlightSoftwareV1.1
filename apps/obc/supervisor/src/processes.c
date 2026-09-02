#include "processes.h"

#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <libgen.h>
#include <stdio.h>
#include <sys/wait.h>
#include <time.h>
#include "pthread.h"
#include "string.h"

#include "obc_ipc.h"

#define NUM_PROCESSES (sizeof(processes) / sizeof(processes[0]))
#define HEARTBEAT_TIMEOUT_SEC 5

static struct timespec last_heartbeat[ROLE_TIME + 1]; // indexed by OBC_Roles_t
static pthread_mutex_t hb_lock = PTHREAD_MUTEX_INITIALIZER;

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

/* Corrects any directory issues with the processes */
int obc_resolve_sibling(const char *sibling_name, char *out, size_t out_size)
{
    char self_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
    
    if (len < 0) {
        perror("readlink /proc/self/exe");
        return -1;
    }
    self_path[len] = '\0';

    char *dir = dirname(self_path); // modifies self_path in place (glibc)
    int n = snprintf(out, out_size, "%s/%s", dir, sibling_name);
    return (n < 0 || (size_t)n >= out_size) ? -1 : 0;
}

void supervisor_reap(obc_process_t *processes, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (processes[i].pid <= 0) continue;

        int status;
        pid_t rc = waitpid(processes[i].pid, &status, WNOHANG);
        if (rc == 0) continue; // process is alive
        if (rc < 0) { perror("waitpid"); continue; } // unusual (dead process are > 0)

        if (WIFEXITED(status)) {
            fprintf(stderr, "supervisor: %s exited, code %d\n", processes[i].name, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            fprintf(stderr, "supervisor: %s killed by signal %d\n", processes[i].name, WTERMSIG(status));
        }
        processes[i].pid = -1; // dead. ready to restart with backoff
    }
}

void supervisor_mark_alive(OBC_Roles_t role)
{
    pthread_mutex_lock(&hb_lock);
    clock_gettime(CLOCK_MONOTONIC, &last_heartbeat[role]);
    pthread_mutex_unlock(&hb_lock);
}

void supervisor_is_frozen(OBC_Roles_t role)
{
    struct timespec now, seen;
    clock_gettime(CLOCK_MONOTONIC, &now);
    pthread_mutex_lock(&hb_lock);
    seen = last_heartbeat[role];
    pthread_mutex_unlock(&hb_lock);

    double elapsed = (now.tv_sec - seen.tv_sec) + (now.tv_nsec - seen.tv_nsec) / 1e9;
    return elapsed > HEARTBEAT_TIMEOUT_SEC;
}