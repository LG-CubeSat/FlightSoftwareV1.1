#include "processes.h"

#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <libgen.h>
#include <stdio.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include "pthread.h"
#include "string.h"

#include "obc_ipc.h"

#define NUM_PROCESSES (sizeof(processes) / sizeof(processes[0]))
#define HEARTBEAT_TIMEOUT_SEC 5
#define SHUTDOWN_GRACE_SEC 3

static struct timespec last_heartbeat[ROLE_TIME + 1]; // indexed by OBC_Roles_t
static pthread_mutex_t hb_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t proc_lock = PTHREAD_MUTEX_INITIALIZER; // guards .pid across processes[]

static obc_process_t processes[] = {
    { "fdir", "./obc_fdir", -1, ROLE_FDIR },
    { "commands", "./obc_commands", -1, ROLE_COMMANDS },
    { "compute", "./obc_compute", -1, ROLE_COMPUTE },
    { "data", "./obc_data", -1, ROLE_DATA },
    { "mission", "./obc_mission", -1, ROLE_MISSION },
    { "time", "./obc_time", -1, ROLE_TIME },
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
        pthread_mutex_lock(&proc_lock);
        pid_t pid = processes[i].pid;
        pthread_mutex_unlock(&proc_lock);

        if (pid <= 0) continue;

        int status;
        pid_t rc = waitpid(pid, &status, WNOHANG);
        if (rc == 0) continue; // process is alive
        if (rc < 0) { perror("waitpid"); continue; } // unusual (dead process are > 0)

        if (WIFEXITED(status)) {
            fprintf(stderr, "supervisor: %s exited, code %d\n", processes[i].name, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            fprintf(stderr, "supervisor: %s killed by signal %d\n", processes[i].name, WTERMSIG(status));
        }

        pthread_mutex_lock(&proc_lock);
        processes[i].pid = -1; // dead. ready to restart with backoff
        pthread_mutex_unlock(&proc_lock);
    }
}

void supervisor_heartbeat(void)
{
    supervisor_reap(processes, NUM_PROCESSES);
}

/* Sends SIGTERM and waits up to SHUTDOWN_GRACE_SEC for a clean exit,
   escalating to SIGKILL if the process ignores it. Blocks until gone. */
int supervisor_shutdown_process(obc_process_t *proc)
{
    pthread_mutex_lock(&proc_lock);
    pid_t pid = proc->pid;
    pthread_mutex_unlock(&proc_lock);

    if (pid <= 0) {
        return 0; // already dead
    }

    if (kill(pid, SIGTERM) != 0 && errno != ESRCH) {
        perror("kill(SIGTERM)");
        return -1;
    }

    struct timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec += SHUTDOWN_GRACE_SEC;

    for (;;) {
        int status;
        pid_t rc = waitpid(pid, &status, WNOHANG);
        if (rc == pid) {
            pthread_mutex_lock(&proc_lock);
            proc->pid = -1;
            pthread_mutex_unlock(&proc_lock);
            return 0;
        }
        if (rc < 0) {
            perror("waitpid");
            return -1;
        }

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec > deadline.tv_sec ||
            (now.tv_sec == deadline.tv_sec && now.tv_nsec > deadline.tv_nsec)) {
            break; // grace period expired, still alive
        }

        struct timespec poll_interval = { .tv_sec = 0, .tv_nsec = 50000000 }; // 50ms
        nanosleep(&poll_interval, NULL);
    }

    fprintf(stderr, "supervisor: %s ignored SIGTERM, sending SIGKILL\n", proc->name);
    kill(pid, SIGKILL);

    int status;
    waitpid(pid, &status, 0); // SIGKILL can't be caught or blocked, this returns quickly
    pthread_mutex_lock(&proc_lock);
    proc->pid = -1;
    pthread_mutex_unlock(&proc_lock);
    return 0;
}

/* Shuts the process down if still running, then spawns a fresh copy. */
int supervisor_restart_process(obc_process_t *proc)
{
    if (supervisor_shutdown_process(proc) != 0) {
        return -1;
    }

    char *argv[] = { (char *)proc->path, NULL };
    pid_t pid;
    int rc = posix_spawn(&pid, proc->path, NULL, NULL, argv, environ);
    if (rc != 0) {
        fprintf(stderr, "supervisor: failed to restart %s: %s\n", proc->name, strerror(rc));
        return -1;
    }

    pthread_mutex_lock(&proc_lock);
    proc->pid = pid;
    pthread_mutex_unlock(&proc_lock);

    fprintf(stderr, "supervisor: restarted %s (pid %d)\n", proc->name, pid);
    return 0;
}

obc_process_t *supervisor_find_process(OBC_Roles_t role)
{
    for (size_t i = 0; i < NUM_PROCESSES; i++) {
        if (processes[i].role == role) {
            return &processes[i];
        }
    }
    return NULL;
}

void supervisor_mark_alive(OBC_Roles_t role)
{
    pthread_mutex_lock(&hb_lock);
    clock_gettime(CLOCK_MONOTONIC, &last_heartbeat[role]);
    pthread_mutex_unlock(&hb_lock);
}

int supervisor_is_frozen(OBC_Roles_t role)
{
    struct timespec now, seen;
    clock_gettime(CLOCK_MONOTONIC, &now);
    pthread_mutex_lock(&hb_lock);
    seen = last_heartbeat[role];
    pthread_mutex_unlock(&hb_lock);

    double elapsed = (now.tv_sec - seen.tv_sec) + (now.tv_nsec - seen.tv_nsec) / 1e9;
    return elapsed > HEARTBEAT_TIMEOUT_SEC;
}