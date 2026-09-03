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

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#define NUM_PROCESSES (sizeof(processes) / sizeof(processes[0]))
#define HEARTBEAT_TIMEOUT_SEC 5
#define SHUTDOWN_GRACE_SEC 3

static struct timespec last_heartbeat[ROLE_TIME + 1]; // indexed by OBC_Roles_t
static pthread_mutex_t hb_lock = PTHREAD_MUTEX_INITIALIZER; // guards the heartbeat timestamp updates
static pthread_mutex_t proc_lock = PTHREAD_MUTEX_INITIALIZER; // guards .pid across processes[]
static volatile sig_atomic_t shutting_down = 0; // set by supervisor_shutdown_all

static void supervisor_check_frozen(void);
int supervisor_is_frozen(OBC_Roles_t role);

static obc_process_t processes[] = {
    { "fdir", "obc_fdir", {0}, -1, ROLE_FDIR },
    { "commands", "obc_commands", {0}, -1, ROLE_COMMANDS },
    { "compute", "obc_compute", {0}, -1, ROLE_COMPUTE },
    { "data", "obc_data", {0}, -1, ROLE_DATA },
    { "mission", "obc_mission", {0}, -1, ROLE_MISSION },
    { "time", "obc_time", {0}, -1, ROLE_TIME },
};

/* Finds the directory this supervisor binary itself is running from,
   and fills `out` with "<that dir>/sibling_name". Linux gets it from
   /proc/self/exe; macOS has no /proc, so it uses _NSGetExecutablePath. */
static int obc_resolve_sibling(const char *sibling_name, char *out, size_t out_size)
{
    char self_path[PATH_MAX];

#if defined(__APPLE__)
    uint32_t size = sizeof(self_path);
    if (_NSGetExecutablePath(self_path, &size) != 0) {
        fprintf(stderr, "supervisor: executable path too long for buffer\n");
        return -1;
    }
#else
    ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
    if (len < 0) {
        perror("readlink /proc/self/exe");
        return -1;
    }
    self_path[len] = '\0';
#endif

    char *dir = dirname(self_path); // modifies self_path in place
    int n = snprintf(out, out_size, "%s/%s", dir, sibling_name);
    return (n < 0 || (size_t)n >= out_size) ? -1 : 0;
}

/* Resolves every sibling binary's path relative to supervisor's own
   location, so start_all_processes works regardless of launch cwd. */
int supervisor_resolve_paths(void)
{
    for (size_t i = 0; i < NUM_PROCESSES; i++) {
        if (obc_resolve_sibling(processes[i].exe_name, processes[i].resolved_path,
                                 sizeof(processes[i].resolved_path)) != 0) {
            fprintf(stderr, "supervisor: failed to resolve path for %s\n", processes[i].name);
            return -1;
        }
    }
    return 0;
}

/* Starts the processes of the OBC */
int start_all_processes(void)
{
    if (supervisor_resolve_paths() != 0) {
        return -1;
    }

    for (size_t i = 0; i < NUM_PROCESSES; i++) {
        char *argv[] = { processes[i].resolved_path, NULL };
        int rc = posix_spawn(&processes[i].pid, processes[i].resolved_path,
            NULL, NULL, argv, environ);

        if (rc != 0) {
            fprintf(stderr, "supervisor: failed to spawn %s: %s\n", processes[i].name, strerror(rc));
            return -1;
        }
        supervisor_mark_alive(processes[i].role); // grace period before it's judged frozen
    }
    return 0;
}

void supervisor_reap(obc_process_t *processes_to_reap, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        pthread_mutex_lock(&proc_lock);
        pid_t pid = processes_to_reap[i].pid;
        pthread_mutex_unlock(&proc_lock);

        if (pid <= 0) continue;

        int status;
        pid_t rc = waitpid(pid, &status, WNOHANG);
        if (rc == 0) continue; // process is alive
        if (rc < 0) { perror("waitpid"); continue; } // unusual (dead process are > 0)

        if (WIFEXITED(status)) {
            fprintf(stderr, "supervisor: %s exited, code %d\n", processes_to_reap[i].name, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            fprintf(stderr, "supervisor: %s killed by signal %d\n", processes_to_reap[i].name, WTERMSIG(status));
        }

        pthread_mutex_lock(&proc_lock);
        processes_to_reap[i].pid = -1; // dead. ready to restart with backoff
        pthread_mutex_unlock(&proc_lock);
    }
}

void supervisor_heartbeat(void)
{
    supervisor_reap(processes, NUM_PROCESSES); // query if running through pid_wait
    supervisor_check_frozen(); // check if stuck through a timestamp
}

/* Restarts any live process (pid > 0) that's gone quiet too long.
   Already-dead slots are supervisor_reap's job, not this one's. */
static void supervisor_check_frozen(void)
{
    if (shutting_down) {
        return;
    }

    for (size_t i = 0; i < NUM_PROCESSES; i++) {
        pthread_mutex_lock(&proc_lock);
        pid_t pid = processes[i].pid;
        pthread_mutex_unlock(&proc_lock);

        if (pid <= 0) continue;

        if (supervisor_is_frozen(processes[i].role)) {
            fprintf(stderr, "supervisor: %s appears frozen, restarting\n", processes[i].name);
            supervisor_restart_process(&processes[i]);
        }
    }
}

/* Signals intent to shut everything down, then does it -- the flag stops
   supervisor_check_frozen from "helpfully" restarting something mid-shutdown. */
void supervisor_shutdown_all(void)
{
    shutting_down = 1;
    for (size_t i = 0; i < NUM_PROCESSES; i++) {
        supervisor_shutdown_process(&processes[i]);
    }
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

    struct timespec deadline; // needs deadline to know when to forcefully shutdown
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec += SHUTDOWN_GRACE_SEC; // X seconds till deadline

    for (;;) {
        int status;
        pid_t rc = waitpid(pid, &status, WNOHANG);
        if (rc == pid) {
            pthread_mutex_lock(&proc_lock);
            proc->pid = -1;
            pthread_mutex_unlock(&proc_lock);
            return 0; // success
        }
        if (rc < 0) {
            perror("waitpid");
            return -1; // error
        }

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec > deadline.tv_sec ||
            (now.tv_sec == deadline.tv_sec && now.tv_nsec > deadline.tv_nsec)) {
            break; // grace period expired, still alive
        }

        // delays until trying again. tv_nsec is nano seconds.
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
    return 0; // forceful shutdown
}

/* Shuts the process down if still running, then spawns a fresh copy. */
int supervisor_restart_process(obc_process_t *proc)
{
    if (supervisor_shutdown_process(proc) != 0) {
        return -1;
    }

    char *argv[] = { proc->resolved_path, NULL };
    pid_t pid;
    int rc = posix_spawn(&pid, proc->resolved_path, NULL, NULL, argv, environ);
    if (rc != 0) {
        fprintf(stderr, "supervisor: failed to restart %s: %s\n", proc->name, strerror(rc));
        return -1;
    }

    pthread_mutex_lock(&proc_lock);
    proc->pid = pid;
    pthread_mutex_unlock(&proc_lock);

    supervisor_mark_alive(proc->role); // grace period before it's judged frozen again

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
    clock_gettime(CLOCK_MONOTONIC, &last_heartbeat[role]); // writes the monotonic to the last_heartbeat
    pthread_mutex_unlock(&hb_lock);
}

int supervisor_is_frozen(OBC_Roles_t role)
{
    // checks the heartbeat timestamps and checks if its been too long since hearing from a process
    struct timespec now, seen;
    clock_gettime(CLOCK_MONOTONIC, &now);
    pthread_mutex_lock(&hb_lock);
    seen = last_heartbeat[role];
    pthread_mutex_unlock(&hb_lock);

    double elapsed = (now.tv_sec - seen.tv_sec) + (now.tv_nsec - seen.tv_nsec) / 1e9;
    return elapsed > HEARTBEAT_TIMEOUT_SEC;
}