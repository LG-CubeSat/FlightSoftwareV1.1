/*
 * Multi-node addressing test for the comms bus.
 *
 * Spawns one master (OBC, addr=1) and two slaves (ADCS addr=2, EPS addr=3),
 * all sharing the real platform/sim/drivers/comms_i2c.c implementation --
 * not a mock. The master broadcasts every frame to every connected slave
 * (see comms_i2c.c's send()); each slave's receive() is responsible for
 * discarding anything not addressed to it. This test exists specifically
 * to prove that discard behavior actually works: with only two nodes
 * (test_comms_bus.c), there's no second slave for a misaddressed frame to
 * accidentally end up at, so that path has never actually been exercised
 * until now.
 *
 * OBC sends one message addressed to ADCS and a different one addressed
 * to EPS. Both slaves physically receive both frames (that's the
 * broadcast part) -- the test passes only if each slave ends up with
 * exactly its own message, not the other's.
 *
 * Sabotage-verified manually: commenting out the
 * `if (frame.dest_addr != my_bus_address)` check in comms_bus_receive()
 * (platform/sim/drivers/comms_i2c.c) makes this test fail -- confirms it's
 * actually exercising the filter, not just accidentally passing.
 *
 * Run directly:
 *   ./build/tests/comms_bus_addressing_test
 * Or via CTest:
 *   ctest --test-dir build -R comms_bus_addressing_test --output-on-failure
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "comms_bus.h"

#define OBC_ADDR  1
#define ADCS_ADDR 2
#define EPS_ADDR  3

#define MSG_FOR_ADCS "for ADCS only"
#define MSG_FOR_EPS  "for EPS only"
#define TEST_TIMEOUT_SEC 5

static void fail(const char *who, const char *what) {
    fprintf(stderr, "[%s] FAIL: %s\n", who, what);
    fflush(stderr);
    _exit(1);
}

static void run_master(void) {
    alarm(TEST_TIMEOUT_SEC);

    CommsBus_t bus = create_comms_bus();
    if (bus.initialize(OBC_ADDR, 1) != COMMS_BUS_OK) {
        fail("OBC", "comms_bus_initialize failed");
    }

    /* comms_bus_send()'s own internal retry only waits for the FIRST
     * connection to exist -- with two slaves connecting independently,
     * give both a moment to finish before sending either message. */
    sleep(1);

    int sent_adcs = bus.send(ADCS_ADDR, (const uint8_t *)MSG_FOR_ADCS, (uint16_t)strlen(MSG_FOR_ADCS));
    if (sent_adcs != (int)strlen(MSG_FOR_ADCS)) {
        fail("OBC", "send to ADCS did not return the expected length");
    }

    int sent_eps = bus.send(EPS_ADDR, (const uint8_t *)MSG_FOR_EPS, (uint16_t)strlen(MSG_FOR_EPS));
    if (sent_eps != (int)strlen(MSG_FOR_EPS)) {
        fail("OBC", "send to EPS did not return the expected length");
    }

    printf("[OBC]  PASS: sent both addressed messages\n");
    fflush(stdout);
    _exit(0);
}

/* Slaves see every broadcast frame, including ones addressed to the other
 * slave -- receive() correctly discards those (returns 0), but the caller
 * still has to ask again for the real one. Loop until a real match shows
 * up; alarm() is the timeout safety net, same as every other test here. */
static int receive_for_me(CommsBus_t *bus, uint8_t *src_addr_out, uint8_t *buf, uint16_t buf_len) {
    while (1) {
        int n = bus->receive(src_addr_out, buf, buf_len);
        if (n > 0) {
            return n;
        }
        if (n < 0) {
            return n;
        }
        /* n == 0: not addressed to us, try again */
    }
}

static void run_adcs(void) {
    alarm(TEST_TIMEOUT_SEC);

    CommsBus_t bus = create_comms_bus();
    if (bus.initialize(ADCS_ADDR, 0) != COMMS_BUS_OK) {
        fail("ADCS", "comms_bus_initialize failed");
    }

    uint8_t buf[128] = {0};
    uint8_t src_addr = 0;
    int received = receive_for_me(&bus, &src_addr, buf, sizeof(buf));
    if (received != (int)strlen(MSG_FOR_ADCS) ||
        memcmp(buf, MSG_FOR_ADCS, (size_t)received) != 0) {
        fail("ADCS", "did not receive the message addressed to it (got EPS's instead, or nothing)");
    }
    if (src_addr != OBC_ADDR) {
        fail("ADCS", "received message reports the wrong source address");
    }

    printf("[ADCS] PASS: received its own message, not EPS's\n");
    fflush(stdout);
    _exit(0);
}

static void run_eps(void) {
    alarm(TEST_TIMEOUT_SEC);

    CommsBus_t bus = create_comms_bus();
    if (bus.initialize(EPS_ADDR, 0) != COMMS_BUS_OK) {
        fail("EPS", "comms_bus_initialize failed");
    }

    uint8_t buf[128] = {0};
    uint8_t src_addr = 0;
    int received = receive_for_me(&bus, &src_addr, buf, sizeof(buf));
    if (received != (int)strlen(MSG_FOR_EPS) ||
        memcmp(buf, MSG_FOR_EPS, (size_t)received) != 0) {
        fail("EPS", "did not receive the message addressed to it (got ADCS's instead, or nothing)");
    }
    if (src_addr != OBC_ADDR) {
        fail("EPS", "received message reports the wrong source address");
    }

    printf("[EPS]  PASS: received its own message, not ADCS's\n");
    fflush(stdout);
    _exit(0);
}

int main(void) {
    /* Same hardcoded /tmp/comms_i2c.sock as production code -- don't run
     * this alongside a real obc_sim/adcs_sim or the other bus tests. */
    unlink("/tmp/comms_i2c.sock");

    pid_t adcs_pid = fork();
    if (adcs_pid < 0) { perror("fork"); return 1; }
    if (adcs_pid == 0) run_adcs();

    pid_t eps_pid = fork();
    if (eps_pid < 0) { perror("fork"); return 1; }
    if (eps_pid == 0) run_eps();

    usleep(50000); /* give both slaves a head start on connecting */

    pid_t obc_pid = fork();
    if (obc_pid < 0) { perror("fork"); return 1; }
    if (obc_pid == 0) run_master();

    int adcs_status = 0, eps_status = 0, obc_status = 0;
    waitpid(adcs_pid, &adcs_status, 0);
    waitpid(eps_pid, &eps_status, 0);
    waitpid(obc_pid, &obc_status, 0);

    int adcs_ok = WIFEXITED(adcs_status) && WEXITSTATUS(adcs_status) == 0;
    int eps_ok = WIFEXITED(eps_status) && WEXITSTATUS(eps_status) == 0;
    int obc_ok = WIFEXITED(obc_status) && WEXITSTATUS(obc_status) == 0;

    if (adcs_ok && eps_ok && obc_ok) {
        printf("comms_bus_addressing_test: PASS\n");
        return 0;
    }

    fprintf(stderr, "comms_bus_addressing_test: FAIL (adcs_ok=%d eps_ok=%d obc_ok=%d)\n",
            adcs_ok, eps_ok, obc_ok);
    return 1;
}
