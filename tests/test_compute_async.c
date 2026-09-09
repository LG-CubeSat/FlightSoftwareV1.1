/*
 * Integration test for compute's async job engine: the two behaviors that
 * make it different from every other request/reply role in this codebase.
 *   1. A second concurrent compress request gets rejected with
 *      COMPUTE_STATUS_BUSY while one job is already running (not queued).
 *   2. An in-flight job can be cancelled mid-run instead of having to run
 *      to completion.
 *
 * BUSY rejection is keyed on "is a job already running", not on who's
 * asking -- testing it for real needs a second requester with a different
 * role than the first, so this forks subprocesses each bound to a
 * different internal role, the same technique test_comms_bus_addressing.c
 * uses for multiple CSP addresses. Real obc_data and obc_compute binaries
 * are spawned; COMPUTE_CHUNK_DELAY_MS widens the job's runtime so both
 * checks have a reliable window to land in -- production default is 0 (no
 * delay) unless the env var is set, see compute.c.
 *
 * Run directly:
 *   ./build/tests/compute_async_test
 * Or via CTest:
 *   ctest --test-dir build -R compute_async_test --output-on-failure
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>

#include "obc_ipc.h"
#include "obc_compute_protocol.h"

#define TEST_TIMEOUT_SEC 30
#define CHUNK_DELAY_MS   "150"
#define MID_JOB_DELAY_USEC 200000 /* fire the 2nd request / cancel ~200ms into a job that takes ~750ms at 150ms/chunk */

/* Deliberately small: this test races a second job's traffic against the
 * first's on the same listening socket, and every chunk is one more
 * connection contending for obc_ipc's backlog -- keeping the chunk count
 * low (a couple of read chunks, a couple of write chunks) keeps that
 * self-induced contention within what data's bounded retry can absorb.
 * A real job never has this problem (mission only ever runs one at a
 * time); this is purely to keep the test itself non-flaky. */
#define TEST_FILE  "/tmp/compute_async_test_input.txt"
#define TEST_FILE_SIZE 300
#define OUT_1 "/tmp/compute_async_test_out1.rice"
#define OUT_2 "/tmp/compute_async_test_out2.rice"
#define OUT_3 "/tmp/compute_async_test_out3.rice"

static int total_checks = 0;
static int failed_checks = 0;

static void check(const char *label, int cond) {
    total_checks++;
    printf("[CHECK] %s: %s\n", cond ? "PASS" : "FAIL", label);
    if (!cond) failed_checks++;
}

static pid_t spawn_logged(const char *path, const char *log_path, int set_chunk_delay) {
    int fd = open(log_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { perror("open"); return -1; }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); close(fd); return -1; }

    if (pid == 0) {
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);

        if (set_chunk_delay) {
            setenv("COMPUTE_CHUNK_DELAY_MS", CHUNK_DELAY_MS, 1);
        }

        execl(path, path, (char *)NULL);
        perror("execl");
        _exit(127);
    }

    close(fd);
    return pid;
}

/* IPC_send is deliberately fail-fast and never retries a connect()
 * failure (see obc_ipc.c's own comment) -- fine for a lone sender, but
 * this test deliberately races two jobs' traffic against the same
 * listening socket, and a momentarily-full connection backlog can fail
 * a single send attempt (the same class of issue filesystem.c's
 * ipc_send_retrying fixed for data's read-streaming loop). Real
 * production callers like payload_commander_compress_photo don't need
 * this -- mission only ever runs one job at a time -- but this test's
 * whole point is to race two, so it needs to be robust to the resulting
 * transient contention itself. */
#define TEST_IPC_SEND_MAX_RETRIES 20
#define TEST_IPC_SEND_RETRY_DELAY_USEC 5000
static void ipc_send_retrying(OBC_Roles_t dest, const uint8_t *data, uint16_t length) {
    for (int attempt = 0; attempt < TEST_IPC_SEND_MAX_RETRIES; attempt++) {
        if (IPC_send(dest, data, length) >= 0) return;
        usleep(TEST_IPC_SEND_RETRY_DELAY_USEC);
    }
}

/* Forked helper: bind as `role`, send one compress request, write the
 * resulting compute_status_t (or 255 on a malformed/missing reply) as a
 * single byte to result_fd, then exit. Never returns. */
static void run_compress_client(OBC_Roles_t role, uint32_t job_id, const char *out_path, int result_fd) {
    if (IPC_initialize(role) != 0) { _exit(1); }

    compute_compress_request_t req = {0};
    req.job_id = job_id;
    snprintf(req.in_path, sizeof(req.in_path), "%s", TEST_FILE);
    snprintf(req.out_path, sizeof(req.out_path), "%s", out_path);
    req.sample_width = 1;

    ipc_send_retrying(ROLE_COMPUTE, (const uint8_t *)&req, sizeof(req));

    uint8_t buf[sizeof(compute_result_t)];
    OBC_Roles_t src;
    int len = IPC_receive(&src, buf, sizeof(buf));

    uint8_t status = 255; /* sentinel: no valid reply */
    if (len == sizeof(compute_result_t)) {
        compute_result_t result;
        memcpy(&result, buf, sizeof(result));
        if (result.job_id == job_id) status = (uint8_t)result.status;
    }

    write(result_fd, &status, 1);
    _exit(0);
}

/* Forked helper: bind as `role`, send one cancel request, exit. Fire and
 * forget -- nothing replies to a cancel. */
static void run_cancel_client(OBC_Roles_t role, uint32_t job_id) {
    if (IPC_initialize(role) != 0) { _exit(1); }
    compute_cancel_request_t req = { .job_id = job_id };
    ipc_send_retrying(ROLE_COMPUTE, (const uint8_t *)&req, sizeof(req));
    _exit(0);
}

static uint8_t wait_for_status(int read_fd, pid_t pid) {
    uint8_t status = 255;
    ssize_t n = read(read_fd, &status, 1);
    (void)n;
    close(read_fd);
    waitpid(pid, NULL, 0);
    return status;
}

int main(void) {
    alarm(TEST_TIMEOUT_SEC);

    unlink("/tmp/comms_i2c.sock");
    unlink("/tmp/obc_ipc_data.sock");
    unlink("/tmp/obc_ipc_compute.sock");
    unlink("/tmp/obc_ipc_mission.sock");
    unlink("/tmp/obc_ipc_fdir.sock");
    unlink(TEST_FILE);
    unlink(OUT_1);
    unlink(OUT_2);
    unlink(OUT_3);

    /* Deterministic, incompressible-ish input -- large enough that at
     * CHUNK_DELAY_MS per chunk the whole job takes over a second, giving
     * both scenarios below a wide, reliable window to land in. */
    FILE * f = fopen(TEST_FILE, "wb");
    if (f == NULL) { fprintf(stderr, "compute_async_test: FAIL (could not create input file)\n"); return 1; }
    srand(7);
    for (int i = 0; i < TEST_FILE_SIZE; i++) fputc(rand() & 0xFF, f);
    fclose(f);

    pid_t data_pid = spawn_logged(DATA_PATH, "/tmp/compute_async_test_data.log", 0);
    pid_t compute_pid = spawn_logged(COMPUTE_PATH, "/tmp/compute_async_test_compute.log", 1);
    if (data_pid < 0 || compute_pid < 0) {
        fprintf(stderr, "compute_async_test: FAIL (could not spawn data/compute)\n");
        return 1;
    }
    usleep(500000); /* let both come up and bind their IPC sockets */

    /* --- Scenario 1: BUSY rejection --- */
    int pipe1[2], pipe2[2];
    pipe(pipe1);
    pipe(pipe2);

    pid_t job1_pid = fork();
    if (job1_pid == 0) {
        close(pipe1[0]);
        run_compress_client(ROLE_MISSION, 1, OUT_1, pipe1[1]);
    }
    close(pipe1[1]);

    usleep(MID_JOB_DELAY_USEC);

    pid_t job2_pid = fork();
    if (job2_pid == 0) {
        close(pipe2[0]);
        run_compress_client(ROLE_FDIR, 2, OUT_2, pipe2[1]);
    }
    close(pipe2[1]);

    uint8_t status2 = wait_for_status(pipe2[0], job2_pid); /* job2 should resolve fast (immediate BUSY reply) */
    uint8_t status1 = wait_for_status(pipe1[0], job1_pid); /* job1 runs to completion */

    check("job1 (first, no contention) completes OK", status1 == COMPUTE_STATUS_OK);
    check("job2 (fired while job1 running) gets BUSY", status2 == COMPUTE_STATUS_BUSY);

    /* --- Scenario 2: cancellation --- */
    int pipe3[2];
    pipe(pipe3);

    pid_t job3_pid = fork();
    if (job3_pid == 0) {
        close(pipe3[0]);
        run_compress_client(ROLE_MISSION, 3, OUT_3, pipe3[1]);
    }
    close(pipe3[1]);

    usleep(MID_JOB_DELAY_USEC);

    pid_t cancel_pid = fork();
    if (cancel_pid == 0) {
        run_cancel_client(ROLE_FDIR, 3);
    }
    waitpid(cancel_pid, NULL, 0);

    uint8_t status3 = wait_for_status(pipe3[0], job3_pid);
    check("job3 (cancelled mid-run) gets CANCELLED", status3 == COMPUTE_STATUS_CANCELLED);

    kill(data_pid, SIGTERM);
    kill(compute_pid, SIGTERM);
    waitpid(data_pid, NULL, 0);
    waitpid(compute_pid, NULL, 0);

    if (failed_checks == 0) {
        printf("compute_async_test: PASS (%d/%d checks)\n", total_checks, total_checks);
        return 0;
    }

    fprintf(stderr, "compute_async_test: FAIL (%d/%d checks failed)\n", failed_checks, total_checks);
    fprintf(stderr, "  See /tmp/compute_async_test_{data,compute}.log for full captured output.\n");
    return 1;
}
