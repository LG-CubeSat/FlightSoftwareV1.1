/*
 * Integration test for the ACK/NACK reply layered on top of the command
 * envelope:
 *   test client sends a raw command_envelope_t (sometimes deliberately
 *   malformed or unrecognized) -> ADCS command_handler validates it and
 *   replies command_ack_t on the same connection, before dispatching
 *   anything further.
 *
 * position_command_test.c already covers the ACK happy path (every valid
 * command gets ACKed, repeatedly, alongside the full task chain). This
 * test covers what that one doesn't: the NACK side of the contract, and
 * that a bad command doesn't wedge the handler for the next, valid one.
 *
 * There is no more standalone "obc_sim" binary -- see position_command_test.c
 * for why this test is itself the OBC's CSP node (csp_network_init(OBC_ADDRESS, 1))
 * rather than spawning one. Only the real adcs_sim binary is spawned, since
 * command_handler.c's logic can't be exercised without it.
 *
 * ADCS_SIM_PATH is injected by tests/CMakeLists.txt via $<TARGET_FILE:...>,
 * so this always exercises the binary actually built by this build,
 * wherever the build directory lives.
 *
 * Run directly:
 *   ./build/tests/command_ack_test
 * Or via CTest:
 *   ctest --test-dir build -R command_ack_test --output-on-failure
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>

#include <csp/csp.h>
#include "csp_network.h"
#include "csp_commands.h"

#define TEST_TIMEOUT_SEC 20
#define UNKNOWN_COMMAND_ID 99

#define ADCS_LOG "/tmp/command_ack_test_adcs.log"

static int total_checks = 0;
static int failed_checks = 0;

static char * slurp_file(const char * path) {
    FILE * f = fopen(path, "r");
    if (f == NULL) {
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < 0) {
        fclose(f);
        return NULL;
    }

    char * buf = malloc((size_t)size + 1);
    if (buf == NULL) {
        fclose(f);
        return NULL;
    }

    size_t read_bytes = fread(buf, 1, (size_t)size, f);
    buf[read_bytes] = '\0';
    fclose(f);
    return buf;
}

static void check_not_contains(const char * label, const char * haystack, const char * marker) {
    total_checks++;
    if (haystack == NULL || strstr(haystack, marker) == NULL) {
        printf("[CHECK] PASS: %s\n", label);
    } else {
        printf("[CHECK] FAIL: %s (unexpectedly found \"%s\")\n", label, marker);
        failed_checks++;
    }
}

static void check_true(const char * label, int condition) {
    total_checks++;
    if (condition) {
        printf("[CHECK] PASS: %s\n", label);
    } else {
        printf("[CHECK] FAIL: %s\n", label);
        failed_checks++;
    }
}

static pid_t spawn_logged(const char * path, const char * log_path) {
    int fd = open(log_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        close(fd);
        return -1;
    }

    if (pid == 0) {
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);

        execl(path, path, (char *)NULL);
        perror("execl");
        _exit(127);
    }

    close(fd);
    return pid;
}

/* Sends exactly `length` bytes as the payload (a raw command_envelope_t by
 * default, but callers can send fewer bytes than that to exercise a
 * malformed request) and reports whether *any* reply arrived, and if so
 * whether it was an ACK or NACK. Returns 1 if a reply arrived, 0 if the
 * read timed out with nothing. */
static int send_raw_and_get_ack(const void * payload, uint16_t length, uint32_t seq, command_ack_status_t * status_out) {
    csp_conn_t * conn = csp_connect(CSP_PRIO_NORM, ADCS_ADDRESS, ADCS_CMD_PORT, 1000, CSP_O_NONE);
    if (conn == NULL) {
        return 0;
    }

    csp_packet_t * packet = csp_buffer_get(0);
    if (packet == NULL) {
        csp_close(conn);
        return 0;
    }

    memcpy(packet->data, payload, length);
    packet->length = length;
    csp_send(conn, packet);

    int got_reply = 0;
    csp_packet_t * reply;
    while ((reply = csp_read(conn, 500)) != NULL) {
        if (reply->length >= sizeof(command_ack_t)) {
            command_ack_t ack;
            memcpy(&ack, reply->data, sizeof(ack));
            if (ack.ack_seq == seq) {
                got_reply = 1;
                if (status_out != NULL) {
                    *status_out = ack.status;
                }
            }
        }
        csp_buffer_free(reply);
    }
    csp_close(conn);
    return got_reply;
}

int main(void) {
    alarm(TEST_TIMEOUT_SEC);

    /* Same hardcoded /tmp/comms_i2c.sock as production code -- don't run
     * this test at the same time as a real obc_supervisor/adcs_sim, or as
     * position_command_test. */
    unlink("/tmp/comms_i2c.sock");
    unlink(ADCS_LOG);

    csp_network_init(OBC_ADDRESS, /* is_master = */ 1);

    pid_t adcs_pid = spawn_logged(ADCS_SIM_PATH, ADCS_LOG);
    if (adcs_pid < 0) {
        fprintf(stderr, "command_ack_test: FAIL (could not spawn adcs_sim)\n");
        return 1;
    }

    usleep(500000); /* give ADCS a moment to come up and connect to the bus master */

    /* 1. Unrecognized command_id -> NACK, not silence. */
    command_envelope_t unknown_cmd = { .command_id = UNKNOWN_COMMAND_ID, .seq = 1 };
    command_ack_status_t status = ACK;
    int got_reply = send_raw_and_get_ack(&unknown_cmd, sizeof(unknown_cmd), 1, &status);
    check_true("Unknown command_id gets a reply", got_reply);
    check_true("Unknown command_id is NACKed", got_reply && status == NACK);

    /* 2. CMD_MOVE_TO_POSITION with the target_position missing (envelope
     * only) -> command_handler.c's own length check must catch it and
     * reply NACK, not read past the buffer or dispatch a garbage target. */
    command_envelope_t short_move_cmd = { .command_id = CMD_MOVE_TO_POSITION, .seq = 2 };
    status = ACK;
    got_reply = send_raw_and_get_ack(&short_move_cmd, sizeof(short_move_cmd), 2, &status);
    check_true("Undersized move command gets a reply", got_reply);
    check_true("Undersized move command is NACKed", got_reply && status == NACK);

    /* 3. A well-formed command right after two bad ones must still be
     * ACKed -- proves the handler isn't wedged or out of reply buffers
     * after rejecting malformed input. */
    position_command_t good_cmd = {
        .envelope = { .command_id = CMD_MOVE_TO_POSITION, .seq = 3 },
        .target_position = 100
    };
    status = NACK;
    got_reply = send_raw_and_get_ack(&good_cmd, sizeof(good_cmd), 3, &status);
    check_true("Valid command after bad ones gets a reply", got_reply);
    check_true("Valid command after bad ones is ACKed", got_reply && status == ACK);

    kill(adcs_pid, SIGTERM);
    waitpid(adcs_pid, NULL, 0);

    char * adcs_log = slurp_file(ADCS_LOG);

    /* ADCS must be able to get a buffer for every reply it sends. */
    check_not_contains("ADCS never fails to get an ACK reply buffer", adcs_log, "Faiuled to get packet buffer");

    free(adcs_log);

    if (failed_checks == 0) {
        printf("command_ack_test: PASS (%d/%d checks)\n", total_checks, total_checks);
        return 0;
    }

    fprintf(stderr, "command_ack_test: FAIL (%d/%d checks failed)\n", failed_checks, total_checks);
    fprintf(stderr, "  See %s for full captured ADCS output.\n", ADCS_LOG);
    return 1;
}
