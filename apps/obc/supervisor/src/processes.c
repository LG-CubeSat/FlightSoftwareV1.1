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
#define MAX_CRASH_RESTARTS 5

static struct timespec last_heartbeat[ROLE_COUNT]; // indexed by OBC_Roles_t
static pthread_mutex_t hb_lock = PTHREAD_MUTEX_INITIALIZER; // guards the heartbeat timestamp updates
static pthread_mutex_t proc_lock = PTHREAD_MUTEX_INITIALIZER; // guards .pid across processes[]
static volatile sig_atomic_t shutting_down = 0; // set by supervisor_shutdown_all

static void supervisor_check_frozen(void);
int supervisor_is_frozen(OBC_Roles_t role);

static obc_process_t processes[] = {
    { .name="fdir", .exe_name="obc_fdir", .resolved_path={0}, .pid=-1, .role=ROLE_FDIR, .restart_count=0, .lifecycle_lock=PTHREAD_MUTEX_INITIALIZER },
    { .name="commands", .exe_name="obc_commands", .resolved_path={0}, .pid=-1, .role=ROLE_COMMANDS, .restart_count=0, .lifecycle_lock=PTHREAD_MUTEX_INITIALIZER },
    { .name="compute", .exe_name="obc_compute", .resolved_path={0}, .pid=-1, .role=ROLE_COMPUTE, .restart_count=0, .lifecycle_lock=PTHREAD_MUTEX_INITIALIZER},
    { .name="data", .exe_name="obc_data", .resolved_path={0}, .pid=-1, .role=ROLE_DATA, .restart_count=0, .lifecycle_lock=PTHREAD_MUTEX_INITIALIZER },
    { .name="mission", .exe_name="obc_mission", .resolved_path={0}, .pid=-1, .role=ROLE_MISSION, .restart_count=0, .lifecycle_lock=PTHREAD_MUTEX_INITIALIZER },
    { .name="time", .exe_name="obc_time", .resolved_path={0}, .pid=-1, .role=ROLE_TIME, .restart_count=0, .lifecycle_lock=PTHREAD_MUTEX_INITIALIZER },
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
        fprintf(stderr, "[SUPERVISOR] Executable path too long for buffer\n");
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
            fprintf(stderr, "[OBC SUPERVISOR] Failed to resolve path for %s\n", processes[i].name);
            return -1;
        }
    }
    return 0;
}

static int supervisor_shutdown_process_locked(obc_process_t *proc);
static int supervisor_restart_process_locked(obc_process_t *proc);

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
            fprintf(stderr, "[OBC SUPERVISOR] failed to spawn %s: %s\n", processes[i].name, strerror(rc));
            return -1;
        }
        supervisor_mark_alive(processes[i].role); // grace period before it's judged frozen
    }
    return 0;
}

void supervisor_reap(obc_process_t *processes_to_reap, size_t n)
{
    // NOTE: supervisor reap checks if processes are alive or even running.
    for (size_t i = 0; i < n; i++) {
        obc_process_t *proc = &processes_to_reap[i];

        /*
        No shutdown or restart operation may manipulate this process while the reaper is checking or reaping it.
        */
        pthread_mutex_lock(&proc->lifecycle_lock);

        pthread_mutex_lock(&proc_lock);
        pid_t pid = proc->pid;
        pthread_mutex_unlock(&proc_lock);

        if (pid <= 0) {
            pthread_mutex_unlock(&proc->lifecycle_lock);
            continue;
        }

        int status;
        pid_t rc = waitpid(pid, &status, WNOHANG);
        if (rc == 0) { // process is alive
            pthread_mutex_unlock(&proc->lifecycle_lock);
            continue;
        } 
        if (rc < 0) { // unusual (dead process are > 0)
            pthread_mutex_unlock(&proc->lifecycle_lock);
            perror("waitpid"); 
            continue; 
        }

        // in the case those previous if pass the process is Dead.

        if (WIFEXITED(status)) {
            fprintf(stderr, "[OBC SUPERVISOR] %s exited, code %d\n", proc->name, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            fprintf(stderr, "[OBC SUPERVISOR] %s killed by signal %d\n", proc->name, WTERMSIG(status));
        }
        
        pthread_mutex_lock(&proc_lock);
        proc->pid = -1; // dead. ready to restart with backoff
        pthread_mutex_unlock(&proc_lock);

        if (shutting_down) {
            pthread_mutex_unlock(&proc->lifecycle_lock);
            continue;
        }

        proc->restart_count++;

        // TODO: Will eventually want to reset restart count after prolonger healthy running.
        if (proc->restart_count > MAX_CRASH_RESTARTS) {
            fprintf(stderr, "[OBC SUPERVISOR] %s crashed %d times, giving up\n", proc->name, proc->restart_count);
            pthread_mutex_unlock(&proc->lifecycle_lock);
            continue;
        }

        fprintf(stderr, "[OBC SUPERVISOR] Restarting %s after crash (attempt %d/%d)\n", proc->name, proc->restart_count, MAX_CRASH_RESTARTS);
        
        /*
        We already hold this processe's lifecycle lock, so use the internal version. Calling the public wrapper would deadlock.
        */
        supervisor_restart_process(proc);
        pthread_mutex_unlock(&proc->lifecycle_lock);
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
    // NOTE: supervisor check frozen sees if a process is hanging, or is frozen.
    if (shutting_down) {
        return;
    }

    for (size_t i = 0; i < NUM_PROCESSES; i++) {
        /*
        FDIR Owns progress-timeout policy for the other roles.
        Supervisor directly monitors only FDIR.
        */
        if (processes[i].role != ROLE_FDIR) {
            continue;
        }

        pthread_mutex_lock(&proc_lock);
        pid_t pid = processes[i].pid;
        pthread_mutex_unlock(&proc_lock);

        if (pid <= 0) continue;

        if (supervisor_is_frozen(processes[i].role)) {
            fprintf(stderr, "[OBC SUPERVISOR] %s appears frozen, restarting\n", processes[i].name);
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
static int supervisor_shutdown_process_locked(obc_process_t *proc)
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

    fprintf(stderr, "[OBC SUPERVISOR] %s ignored SIGTERM, sending SIGKILL\n", proc->name);
    kill(pid, SIGKILL);

    int status;
    waitpid(pid, &status, 0); // SIGKILL can't be caught or blocked, this returns quickly
    pthread_mutex_lock(&proc_lock);
    proc->pid = -1;
    pthread_mutex_unlock(&proc_lock);
    return 0; // forceful shutdown
}

int supervisor_shutdown_process(obc_process_t *proc)
{
    if (proc == NULL) {
        return -1;
    }

    pthread_mutex_lock(&proc->lifecycle_lock);

    int result = supervisor_shutdown_process_locked(proc);

    pthread_mutex_unlock(&proc->lifecycle_lock);
    return result;
}

/* Shuts the process down if still running, then spawns a fresh copy. */
static int supervisor_restart_process_locked(obc_process_t *proc)
{
    if (supervisor_shutdown_process_locked(proc) != 0) {
        return -1;
    }

    char *argv[] = { proc->resolved_path, NULL };
    pid_t pid;
    int rc = posix_spawn(&pid, proc->resolved_path, NULL, NULL, argv, environ);
    if (rc != 0) {
        fprintf(stderr, "[OBC SUPERVISOR] Failed to restart %s: %s\n", proc->name, strerror(rc));
        return -1;
    }

    pthread_mutex_lock(&proc_lock);
    proc->pid = pid;
    pthread_mutex_unlock(&proc_lock);

    supervisor_mark_alive(proc->role); // grace period before it's judged frozen again

    fprintf(stderr, "[OBC SUPERVISOR] Restarted %s (pid %d)\n", proc->name, pid);
    return 0;
}

int supervisor_restart_process(obc_process_t *proc)
{
    if (proc == NULL) {
        return -1;
    }

    pthread_mutex_lock(&proc->lifecycle_lock);

    int result = supervisor_restart_process_locked(proc);

    pthread_mutex_unlock(&proc->lifecycle_lock);

    return result;
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
    if (!role_is_valid((unsigned)role)) {
        fprintf(stderr, "[SUPERVISOR] ignoring heartbeat for invalid role %d\n", role);
        return;
    }
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