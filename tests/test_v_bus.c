/*
 * Sanity test for the v-bus transport used between the OBC and ADCS
 * processes. Exercises the real platform/sim/spi/v_bus.c implementation
 * over an actual Unix domain socket (not a mock), so it catches real
 * regressions in that code -- e.g. a socket type the host OS doesn't
 * support, a framing bug, a hang on connect/accept.
 *
 * Forks two processes that play the OBC (master) and ADCS (slave) roles,
 * exchange a known message in both directions, and verify the bytes match.
 * A watchdog alarm() fails the test loudly instead of hanging if the bus
 * is broken.
 *
 * Run directly:
 *   ./build/tests/v_bus_test
 * Or via CTest:
 *   ctest --test-dir build -R v_bus_test --output-on-failure
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static void run_master(void) {
    alarm(TEST_TIMEOUT_SEC);

    VBus_t bus = create_v_bus();
    if (bus.initialize(1) != V_BUS_OK) {
        fail("OBC", "v_bus_initialize failed");
    }

    int sent = bus.send((const uint8_t *)OBC_TO_ADCS_MSG, (uint16_t)strlen(OBC_TO_ADCS_MSG));
    if (sent != (int)strlen(OBC_TO_ADCS_MSG)) {
        fail("OBC", "send() did not return the expected length");
    }

    uint8_t buf[128] = {0};
    int received = bus.receive(buf, sizeof(buf));
    if (received != (int)strlen(ADCS_TO_OBC_MSG) ||
        memcmp(buf, ADCS_TO_OBC_MSG, (size_t)received) != 0) {
        fail("OBC", "did not receive the expected reply from ADCS");
    }

    printf("[OBC]  PASS: sent %d bytes, received matching %d-byte reply\n", sent, received);
    fflush(stdout);
    _exit(0);
}

static void run_slave(void) {
    alarm(TEST_TIMEOUT_SEC);

    VBus_t bus = create_v_bus();
    if (bus.initialize(0) != V_BUS_OK) {
        fail("ADCS", "v_bus_initialize failed");
    }

    uint8_t buf[128] = {0};
    int received = bus.receive(buf, sizeof(buf));
    if (received != (int)strlen(OBC_TO_ADCS_MSG) ||
        memcmp(buf, OBC_TO_ADCS_MSG, (size_t)received) != 0) {
        fail("ADCS", "did not receive the expected message from OBC");
    }

    int sent = bus.send((const uint8_t *)ADCS_TO_OBC_MSG, (uint16_t)strlen(ADCS_TO_OBC_MSG));
    if (sent != (int)strlen(ADCS_TO_OBC_MSG)) {
        fail("ADCS", "send() did not return the expected length");
    }

    printf("[ADCS] PASS: received %d bytes, sent matching %d-byte reply\n", received, sent);
    fflush(stdout);
    _exit(0);
}

int main(void) {
    /* Clear any stale socket left behind by a previous crashed run. Note:
     * this uses the same hardcoded /tmp/v_bus.sock path as production code,
     * so don't run this test at the same time as a real obc_sim/adcs_sim. */
    unlink("/tmp/v_bus.sock");

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
        printf("v_bus_test: PASS\n");
        return 0;
    }

    fprintf(stderr, "v_bus_test: FAIL (slave_ok=%d master_ok=%d)\n", slave_ok, master_ok);
    return 1;
}
