#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "obc_data_protocol.h"
#include "obc_ipc.h"

#define TEST_TIMEOUT_SEC 20
#define DATA_LOG "/tmp/data_progress_test_data.log"
#define BLOCKING_FIFO "/tmp/data_progress_test_fifo"

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

static int receive_data_heartbeat(int timeout_ms)
{
    OBC_Roles_t source = 0;
    uint8_t unused = 0;
    int length = IPC_receive_timeout(
        &source,
        &unused,
        sizeof(unused),
        timeout_ms
    );
    return length == 0 && source == ROLE_DATA;
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
        if (receive_data_heartbeat(wait_ms)) {
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

static int wait_for_log(const char *needle, int timeout_ms)
{
    int64_t deadline = monotonic_ms() + timeout_ms;
    while (monotonic_ms() < deadline) {
        if (file_contains(DATA_LOG, needle)) {
            return 1;
        }
        usleep(10000);
    }
    return 0;
}

static pid_t spawn_data(const char *data_path)
{
    int log_fd = open(DATA_LOG, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (log_fd < 0) {
        perror("open data log");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork data");
        close(log_fd);
        return -1;
    }

    if (pid == 0) {
        dup2(log_fd, STDOUT_FILENO);
        dup2(log_fd, STDERR_FILENO);
        close(log_fd);
        setenv("DATA_STORAGE_STALL_TIMEOUT_MS", "1500", 1);
        execl(data_path, data_path, (char *)NULL);
        perror("execl obc_data");
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
    unlink("/tmp/obc_ipc_data.sock");
    unlink(BLOCKING_FIFO);
}

int main(int argc, char **argv)
{
    alarm(TEST_TIMEOUT_SEC);

    if (argc != 2) {
        fprintf(stderr, "usage: %s /path/to/obc_data\n", argv[0]);
        return 2;
    }
    if (access(argv[1], X_OK) != 0) {
        perror("data executable is not accessible");
        return 2;
    }

    cleanup();
    unlink(DATA_LOG);
    if (mkfifo(BLOCKING_FIFO, 0600) != 0) {
        perror("mkfifo");
        return 1;
    }

    if (IPC_initialize(ROLE_SUPERVISOR) != IPC_OK) {
        fprintf(stderr, "data_progress_test: supervisor IPC failed\n");
        cleanup();
        return 1;
    }

    pid_t data_pid = spawn_data(argv[1]);
    if (data_pid < 0) {
        cleanup();
        return 1;
    }

    check("idle data sends heartbeats", receive_data_heartbeat(3000));

    data_read_request_t request = {0};
    snprintf(request.path, sizeof(request.path), "%s", BLOCKING_FIFO);
    check(
        "blocking read request reaches data",
        IPC_send(
            ROLE_DATA,
            (const uint8_t *)&request,
            sizeof(request)
        ) >= 0
    );
    check(
        "data enters the deliberately blocked file stream",
        wait_for_log("[STORAGE] Streaming", 2000)
    );

    int transition_heartbeats = drain_heartbeats_for(3000);
    check(
        "data heartbeats while its most recent progress is fresh",
        transition_heartbeats > 0
    );
    check(
        "data withholds heartbeat after storage stalls",
        !receive_data_heartbeat(1800)
    );
    check(
        "data process remains alive during storage stall",
        kill(data_pid, 0) == 0
    );
    check(
        "data logs the stalled-progress transition",
        file_contains(DATA_LOG, "Progress stalled")
    );

    stop_process(data_pid);
    cleanup();

    if (failed_checks == 0) {
        printf(
            "data_progress_test: PASS (%d/%d checks)\n",
            total_checks,
            total_checks
        );
        return 0;
    }

    fprintf(
        stderr,
        "data_progress_test: FAIL (%d/%d checks failed)\n",
        failed_checks,
        total_checks
    );
    fprintf(stderr, "See %s for Data output.\n", DATA_LOG);
    return 1;
}
