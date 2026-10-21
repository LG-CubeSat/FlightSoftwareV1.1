#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "obc_compute_protocol.h"
#include "obc_ipc.h"

#define TEST_TIMEOUT_SEC 20
#define MISSION_LOG "/tmp/mission_progress_test_mission.log"
#define MISSION_STATE "/tmp/mission_progress_test_state"

static int total_checks = 0;
static int failed_checks = 0;

static void check(const char *label, int condition)
{
    total_checks++;
    printf("[CHECK] %s: %s\n", condition ? "PASS" : "FAIL", label);
    if (!condition) {
        failed_checks++;
    }
}

static int64_t monotonic_ms(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static int receive_mission_heartbeat(int timeout_ms)
{
    OBC_Roles_t source = 0;
    uint8_t unused = 0;
    int length = IPC_receive_timeout(
        &source,
        &unused,
        sizeof(unused),
        timeout_ms
    );
    return length == 0 && source == ROLE_MISSION;
}

static int drain_heartbeats_for(int duration_ms)
{
    int count = 0;
    int64_t deadline = monotonic_ms() + duration_ms;

    while (monotonic_ms() < deadline) {
        int64_t remaining = deadline - monotonic_ms();
        int wait_ms = remaining > 250 ? 250 : (int)remaining;
        if (wait_ms <= 0) {
            break;
        }
        if (receive_mission_heartbeat(wait_ms)) {
            count++;
        }
    }

    return count;
}

static int file_contains(const char *path, const char *needle)
{
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    char line[512];
    int found = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        if (strstr(line, needle) != NULL) {
            found = 1;
            break;
        }
    }
    fclose(file);
    return found;
}

static int read_event(int fd, char expected, int timeout_ms)
{
    struct pollfd descriptor = { .fd = fd, .events = POLLIN };
    int ready = poll(&descriptor, 1, timeout_ms);
    if (ready != 1 || !(descriptor.revents & POLLIN)) {
        return 0;
    }

    char event = 0;
    return read(fd, &event, 1) == 1 && event == expected;
}

static void run_stalled_compute(int event_fd)
{
    if (IPC_initialize(ROLE_COMPUTE) != IPC_OK) {
        _exit(10);
    }
    if (write(event_fd, "R", 1) != 1) {
        _exit(11);
    }

    OBC_Roles_t source = 0;
    compute_compress_request_t request;
    int length = IPC_receive_timeout(
        &source,
        (uint8_t *)&request,
        sizeof(request),
        8000
    );
    if (length != sizeof(request) || source != ROLE_MISSION) {
        _exit(12);
    }
    if (write(event_fd, "C", 1) != 1) {
        _exit(13);
    }

    /* Deliberate sabotage: Compute stays alive but never returns a result. */
    for (;;) {
        sleep(1);
    }
}

static pid_t spawn_mission(const char *mission_path)
{
    int log_fd = open(MISSION_LOG, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (log_fd < 0) {
        perror("open mission log");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork mission");
        close(log_fd);
        return -1;
    }

    if (pid == 0) {
        dup2(log_fd, STDOUT_FILENO);
        dup2(log_fd, STDERR_FILENO);
        close(log_fd);
        setenv("MISSION_ASCENT_WAIT_SEC", "1", 1);
        setenv("MISSION_STATE_PATH", MISSION_STATE, 1);
        setenv("MISSION_PAYLOAD_STALL_TIMEOUT_MS", "1500", 1);
        execl(mission_path, mission_path, (char *)NULL);
        perror("execl obc_mission");
        _exit(127);
    }

    close(log_fd);
    return pid;
}

static void stop_process(pid_t pid)
{
    if (pid <= 0) {
        return;
    }
    kill(pid, SIGTERM);
    waitpid(pid, NULL, 0);
}

static void cleanup(void)
{
    unlink("/tmp/obc_ipc_supervisor.sock");
    unlink("/tmp/obc_ipc_compute.sock");
    unlink("/tmp/obc_ipc_mission.sock");
    unlink(MISSION_STATE);
    unlink(MISSION_STATE ".tmp");
    unlink("/tmp/photos");
    unlink("/tmp/photos.rice");
}

int main(int argc, char **argv)
{
    alarm(TEST_TIMEOUT_SEC);

    if (argc != 2) {
        fprintf(stderr, "usage: %s /path/to/obc_mission\n", argv[0]);
        return 2;
    }
    if (access(argv[1], X_OK) != 0) {
        perror("mission executable is not accessible");
        return 2;
    }

    cleanup();
    unlink(MISSION_LOG);

    int compute_events[2];
    if (pipe(compute_events) != 0) {
        perror("pipe");
        return 1;
    }

    pid_t compute_pid = fork();
    if (compute_pid < 0) {
        perror("fork compute");
        close(compute_events[0]);
        close(compute_events[1]);
        return 1;
    }
    if (compute_pid == 0) {
        close(compute_events[0]);
        run_stalled_compute(compute_events[1]);
    }
    close(compute_events[1]);

    if (!read_event(compute_events[0], 'R', 2000)) {
        fprintf(stderr, "mission_progress_test: fake compute did not become ready\n");
        stop_process(compute_pid);
        close(compute_events[0]);
        cleanup();
        return 1;
    }

    if (IPC_initialize(ROLE_SUPERVISOR) != IPC_OK) {
        fprintf(stderr, "mission_progress_test: supervisor IPC failed\n");
        stop_process(compute_pid);
        close(compute_events[0]);
        cleanup();
        return 1;
    }

    pid_t mission_pid = spawn_mission(argv[1]);
    if (mission_pid < 0) {
        stop_process(compute_pid);
        close(compute_events[0]);
        cleanup();
        return 1;
    }

    check("idle mission sends heartbeats", receive_mission_heartbeat(3000));
    check(
        "mission starts a compression request",
        read_event(compute_events[0], 'C', 6000)
    );

    int transition_heartbeats = drain_heartbeats_for(3000);
    check(
        "mission heartbeats while scheduler progress is recent",
        transition_heartbeats > 0
    );
    check(
        "mission withholds heartbeat after compression wait stalls",
        !receive_mission_heartbeat(1800)
    );
    check(
        "mission process remains alive during scheduler stall",
        kill(mission_pid, 0) == 0
    );
    check(
        "mission logs the stalled-progress transition",
        file_contains(MISSION_LOG, "Progress stalled")
    );

    stop_process(mission_pid);
    stop_process(compute_pid);
    close(compute_events[0]);
    cleanup();

    if (failed_checks == 0) {
        printf(
            "mission_progress_test: PASS (%d/%d checks)\n",
            total_checks,
            total_checks
        );
        return 0;
    }

    fprintf(
        stderr,
        "mission_progress_test: FAIL (%d/%d checks failed)\n",
        failed_checks,
        total_checks
    );
    fprintf(stderr, "See %s for Mission output.\n", MISSION_LOG);
    return 1;
}
