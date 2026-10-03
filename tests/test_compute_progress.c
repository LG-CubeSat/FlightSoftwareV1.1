#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#include "obc_ipc.h"
#include "obc_compute_protocol.h"
#include "obc_data_protocol.h"

#define TEST_TIMEOUT_SEC 20

#define WORKER_STALL_TIMEOUT "1500"
#define REPLY_TIMEOUT "10000"

#define COMPUTE_LOG "/tmp/compute_progress_test_compute.log"

static int total_checks = 0;
static int failed_checks = 0;

static void check(const char *label, int condition)
{
    total_checks++;

    printf(
        "[CHECK] %s: %s\n",
        condition ? "PASS" : "FAIL",
        label
    );

    if (!condition) {
        failed_checks++;
    }
}

static int64_t monotonic_ms(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    
    return (int64_t)now.tv_sec * 1000 +
        now.tv_nsec / 1000000;
}

static int ipc_send_retrying(
    OBC_Roles_t destination,
    const uint8_t *data,
    uint16_t length
)
{
    for (int attempt = 0; attempt < 20; attempt++) {
        if (IPC_send(destination, data, length) >= 0) {
            return 0;
        }

        usleep(5000);
    }

    return -1;
}

static void run_fake_data(int event_fd)
{
    if (IPC_initialize(ROLE_DATA) != IPC_OK) {
        _exit(10);
    }

    if (write(event_fd, "R", 1) != 1) {
        _exit(11);
    }

    OBC_Roles_t source = 0;
    data_read_request_t request;

    int length = IPC_receive_timeout(
        &source,
        (uint8_t *)&request,
        sizeof(request),
        5000
    );

    if (length != sizeof(request) || source != ROLE_COMPUTE) {
        _exit(12);
    }

    data_read_reply_t reply = {
        .status = 0,
        .offset = 0,
        .length = 1,
        .is_last = 0,
    };

    reply.payload[0] = 0xff;

    if (ipc_send_retrying(
        ROLE_COMPUTE,
        (const uint8_t *)&reply,
        sizeof(reply)
    ) != 0) {
        _exit(13);
    }

    if (write(event_fd, "C", 1) != 1) {
        _exit(14);
    }

    /*
    Deliberate sabatage: remain alive but never send the next chunk.
    */
    for (;;) {
        sleep(1);
    }
}

static pid_t spawn_compute(void)
{
    int log_fd = open(
        COMPUTE_LOG,
        O_WRONLY | O_CREAT | O_TRUNC,
        0644
    );

    if (log_fd < 0) {
        perror("open compute log");
        return -1;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        close(log_fd);
        return -1;
    }

    if (pid == 0) {
        dup2(log_fd, STDOUT_FILENO);
        dup2(log_fd, STDERR_FILENO);
        close(log_fd);
        /*
        These variables affect only this child and the compute process
        that replaces it through execl().
        */
       setenv("COMPUTE_WORKER_STALL_TIMEOUT_MS", "1500", 1);
       setenv("COMPUTE_REPLY_TIMEOUT_MS", "10000", 1);

       execl(COMPUTE_PATH, COMPUTE_PATH, (char *)NULL);

       /*
       Reached only if execl fails
       */
      perror("execl obc_compute");
      _exit(127);
    }
    close(log_fd);
    return pid;
}

static void run_requester(void)
{
    if (IPC_initialize(ROLE_MISSION) != IPC_OK) {
        _exit(20);
    }

    compute_compress_request_t request = {0};
    request.job_id = 42;
    snprintf(request.in_path, sizeof(request.in_path), "/tmp/fake-input.jpg");
    snprintf(request.out_path, sizeof(request.out_path), "/tmp/fake-output.ssdv");
    request.sample_width = 1;

    if (ipc_send_retrying(
            ROLE_COMPUTE,
            (const uint8_t *)&request,
            sizeof(request)
        ) != 0) {
        _exit(21);
    }

    _exit(0);
}

static int receive_compute_heartbeat(int timeout_ms)
{
    OBC_Roles_t source = 0;
    uint8_t unused = 0;

    int length = IPC_receive_timeout(
        &source,
        &unused,
        sizeof(unused),
        timeout_ms
    );

    return length == 0 && source == ROLE_COMPUTE;
}

/* Drain every queued heartbeat during the transition to stale. Without
 * this, a heartbeat sent just before the deadline could remain queued and
 * make the later silence check look like a new heartbeat. */
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

        if (receive_compute_heartbeat(wait_ms)) {
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

static void stop_process(pid_t pid)
{
    if (pid <= 0) {
        return;
    }

    kill(pid, SIGTERM);
    waitpid(pid, NULL, 0);
}

static void cleanup_socket_paths(void)
{
    unlink("/tmp/obc_ipc_supervisor.sock");
    unlink("/tmp/obc_ipc_compute.sock");
    unlink("/tmp/obc_ipc_data.sock");
    unlink("/tmp/obc_ipc_mission.sock");
}

int main(void)
{
    alarm(TEST_TIMEOUT_SEC);

    cleanup_socket_paths();
    unlink(COMPUTE_LOG);

    int data_events[2];
    if (pipe(data_events) != 0) {
        perror("pipe");
        return 1;
    }

    pid_t data_pid = fork();
    if (data_pid < 0) {
        perror("fork data");
        close(data_events[0]);
        close(data_events[1]);
        return 1;
    }

    if (data_pid == 0) {
        close(data_events[0]);
        run_fake_data(data_events[1]);
    }

    close(data_events[1]);

    /* Wait until fake data has bound its IPC socket. */
    char event = 0;
    if (read(data_events[0], &event, 1) != 1 || event != 'R') {
        fprintf(stderr, "compute_progress_test: fake data did not become ready\n");
        stop_process(data_pid);
        close(data_events[0]);
        cleanup_socket_paths();
        return 1;
    }

    /* The test's parent process plays supervisor so it can observe the
     * real compute heartbeat messages directly. */
    if (IPC_initialize(ROLE_SUPERVISOR) != IPC_OK) {
        fprintf(stderr, "compute_progress_test: supervisor IPC failed\n");
        stop_process(data_pid);
        close(data_events[0]);
        cleanup_socket_paths();
        return 1;
    }

    pid_t compute_pid = spawn_compute();
    if (compute_pid < 0) {
        stop_process(data_pid);
        close(data_events[0]);
        cleanup_socket_paths();
        return 1;
    }

    /* Before any job exists, worker is idle and compute must heartbeat. */
    check(
        "idle compute sends heartbeats",
        receive_compute_heartbeat(3000)
    );

    pid_t requester_pid = fork();
    if (requester_pid < 0) {
        perror("fork requester");
        stop_process(compute_pid);
        stop_process(data_pid);
        close(data_events[0]);
        cleanup_socket_paths();
        return 1;
    }

    if (requester_pid == 0) {
        run_requester();
    }

    int requester_status = 0;
    waitpid(requester_pid, &requester_status, 0);
    check(
        "mission requester submits a compression job",
        WIFEXITED(requester_status) && WEXITSTATUS(requester_status) == 0
    );

    /* Fake data sends one non-final chunk. The worker touches its watch,
     * then waits for another chunk that deliberately never arrives. */
    event = 0;
    check(
        "fake data sends one non-final chunk",
        read(data_events[0], &event, 1) == 1 && event == 'C'
    );

    /* Drain all heartbeats while the 1.5-second progress deadline expires.
     * At least one should arrive while the recent chunk is still healthy. */
    int transition_heartbeats = drain_heartbeats_for(3000);
    check(
        "compute heartbeats while worker progress is recent",
        transition_heartbeats > 0
    );

    /* The queue is drained and the worker is stale. A new heartbeat must
     * not arrive, even though the compute process itself remains alive. */
    check(
        "compute withholds heartbeat after worker progress stalls",
        !receive_compute_heartbeat(1800)
    );
    check(
        "compute process remains alive during worker stall",
        kill(compute_pid, 0) == 0
    );
    check(
        "compute logs the stalled-progress transition",
        file_contains(COMPUTE_LOG, "Progress stalled")
    );

    stop_process(compute_pid);
    stop_process(data_pid);
    close(data_events[0]);
    cleanup_socket_paths();

    if (failed_checks == 0) {
        printf(
            "compute_progress_test: PASS (%d/%d checks)\n",
            total_checks,
            total_checks
        );
        return 0;
    }

    fprintf(
        stderr,
        "compute_progress_test: FAIL (%d/%d checks failed)\n",
        failed_checks,
        total_checks
    );
    fprintf(stderr, "See %s for compute output.\n", COMPUTE_LOG);
    return 1;
}
