/*
 * Sanity test for the comms bus transport used between the OBC and ADCS
 * processes. Exercises the real platform/sim/drivers/comms_i2c.c
 * implementation over an actual Unix domain socket (not a mock), so it
 * catches real regressions in that code -- e.g. a socket type the host OS
 * doesn't support, a framing bug, a hang on connect/accept.
 *
 * Forks two processes that play the OBC (master) and ADCS (slave) roles,
 * exchange a known message in both directions, and verify the bytes match.
 * A watchdog alarm() fails the test loudly instead of hanging if the bus
 * is broken.
 *
 * Run directly:
 *   ./build/tests/comms_bus_test
 * Or via CTest:
 *   ctest --test-dir build -R comms_bus_test --output-on-failure
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

#include "comms_bus.h"

#define OBC_TO_ADCS_MSG "PING from OBC"
#define ADCS_TO_OBC_MSG "PONG from ADCS"
#define TEST_TIMEOUT_SEC 5

static void fail(const char *who, const char *what) {
    fprintf(stderr, "[%s] FAIL: %s\n", who, what);
    fflush(stderr);
    _exit(1);
}

#define OBC_ADDR  1
#define ADCS_ADDR 2

static int64_t monotonic_milliseconds(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return -1;
    }

    return ((int64_t)now.tv_sec * 1000) +
           ((int64_t)now.tv_nsec / 1000000);
}

static void verify_timeout_duration(
    const char *who,
    int64_t started_ms,
    int64_t finished_ms
)
{
    int64_t elapsed_ms = finished_ms - started_ms;

    /*
     * Allow scheduling tolerance while still catching an immediate return
     * or an effectively unbounded wait.
     */
    if (elapsed_ms < (int64_t)(COMMS_BUS_RECEIVE_TIMEOUT_MS / 2U) ||
        elapsed_ms > (int64_t)COMMS_BUS_RECEIVE_TIMEOUT_MS + 500) {
        fail(who, "receive timeout duration was outside the expected range");
    }
}

static void run_master(void) {
    alarm(TEST_TIMEOUT_SEC);

    CommsBus_t bus = create_comms_bus();
    if (bus.initialize(OBC_ADDR, 1) != COMMS_BUS_OK) {
        fail("OBC", "comms_bus_initialize failed");
    }

    /* Let the connected slave exercise an idle receive timeout before the
       first real frame arrives. */
    usleep((COMMS_BUS_RECEIVE_TIMEOUT_MS + 50U) * 1000U);

    int sent = bus.send(ADCS_ADDR, (const uint8_t *)OBC_TO_ADCS_MSG, (uint16_t)strlen(OBC_TO_ADCS_MSG));
    if (sent != (int)strlen(OBC_TO_ADCS_MSG)) {
        fail("OBC", "send() did not return the expected length");
    }

    uint8_t buf[128] = {0};
    uint8_t src_addr = 0;
    int received = bus.receive(&src_addr, buf, sizeof(buf));
    if (received != (int)strlen(ADCS_TO_OBC_MSG) ||
        memcmp(buf, ADCS_TO_OBC_MSG, (size_t)received) != 0) {
        fail("OBC", "did not receive the expected reply from ADCS");
    }

    int64_t timeout_started_ms = monotonic_milliseconds();
    if (timeout_started_ms < 0) {
        fail("OBC", "could not read monotonic clock");
    }

    received = bus.receive(&src_addr, buf, sizeof(buf));

    int64_t timeout_finished_ms = monotonic_milliseconds();
    if (timeout_finished_ms < 0) {
        fail("OBC", "could not read monotonic clock");
    }

    if (received != COMMS_BUS_TIMEOUT) {
        fail("OBC", "idle receive did not return COMMS_BUS_TIMEOUT");
    }

    verify_timeout_duration("OBC", timeout_started_ms, timeout_finished_ms);

    printf("[OBC]  PASS: message exchange and idle timeout succeeded\n");
    fflush(stdout);
    _exit(0);
}

static void run_slave(void) {
    alarm(TEST_TIMEOUT_SEC);

    CommsBus_t bus = create_comms_bus();
    if (bus.initialize(ADCS_ADDR, 0) != COMMS_BUS_OK) {
        fail("ADCS", "comms_bus_initialize failed");
    }

    uint8_t buf[128] = {0};
    uint8_t src_addr = 0;

    int64_t timeout_started_ms = monotonic_milliseconds();
    if (timeout_started_ms < 0) {
        fail("ADCS", "could not read monotonic clock");
    }

    int received = bus.receive(&src_addr, buf, sizeof(buf));

    int64_t timeout_finished_ms = monotonic_milliseconds();
    if (timeout_finished_ms < 0) {
        fail("ADCS", "could not read monotonic clock");
    }

    if (received != COMMS_BUS_TIMEOUT) {
        fail("ADCS", "idle receive did not return COMMS_BUS_TIMEOUT");
    }

    verify_timeout_duration("ADCS", timeout_started_ms, timeout_finished_ms);

    /* The timeout must leave this same connection usable. */
    received = bus.receive(&src_addr, buf, sizeof(buf));
    if (received != (int)strlen(OBC_TO_ADCS_MSG) ||
        memcmp(buf, OBC_TO_ADCS_MSG, (size_t)received) != 0) {
        fail("ADCS", "did not receive the expected message from OBC");
    }

    int sent = bus.send(OBC_ADDR, (const uint8_t *)ADCS_TO_OBC_MSG, (uint16_t)strlen(ADCS_TO_OBC_MSG));
    if (sent != (int)strlen(ADCS_TO_OBC_MSG)) {
        fail("ADCS", "send() did not return the expected length");
    }

    /* Keep the peer open while the master exercises its idle timeout. */
    usleep((COMMS_BUS_RECEIVE_TIMEOUT_MS * 3U) * 1000U);

    printf("[ADCS] PASS: timeout recovery and message exchange succeeded\n");
    fflush(stdout);
    _exit(0);
}

int main(void) {
    /* Clear any stale socket left behind by a previous crashed run. Note:
     * this uses the same hardcoded /tmp/comms_i2c.sock path as production
     * code, so don't run this test at the same time as a real
     * obc_sim/adcs_sim. */
    unlink("/tmp/comms_i2c.sock");

    pid_t slave_pid = fork();
    if (slave_pid < 0) {
        perror("fork");
        return 1;
    }
    if (slave_pid == 0) {
        run_slave();
    }

    usleep(50000); /* give the master a head start on bind()/listen() */

    pid_t master_pid = fork();
    if (master_pid < 0) {
        perror("fork");
        return 1;
    }
    if (master_pid == 0) {
        run_master();
    }

    int slave_status = 0, master_status = 0;
    waitpid(slave_pid, &slave_status, 0);
    waitpid(master_pid, &master_status, 0);

    int slave_ok = WIFEXITED(slave_status) && WEXITSTATUS(slave_status) == 0;
    int master_ok = WIFEXITED(master_status) && WEXITSTATUS(master_status) == 0;

    if (slave_ok && master_ok) {
        printf("comms_bus_test: PASS\n");
        return 0;
    }

    fprintf(stderr, "comms_bus_test: FAIL (slave_ok=%d master_ok=%d)\n", slave_ok, master_ok);
    return 1;
}
