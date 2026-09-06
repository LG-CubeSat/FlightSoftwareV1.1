/*
 * Integration test for the full position-command flow:
 *   test client -> CSP -> comms_bus -> ADCS command_handler -> Command task
 *        -> (Control, Estimation, Sensor, Telemetry tasks; NOT Housekeeping)
 *        -> Telemetry -> CSP -> comms_bus -> test client
 *
 * There is no more standalone "obc_sim" binary to spawn -- the OBC split
 * into 7 processes, and none of them autonomously polls ADCS with position
 * commands the way the old single-binary OBC did. What ADCS actually talks
 * to is a raw CSP node at OBC_ADDRESS; this test *is* that node (same
 * csp_network_init(OBC_ADDRESS, 1) call any real OBC role would make), so
 * it exercises exactly the same wire contract without depending on which
 * internal OBC processes exist. Only the real adcs_sim binary is spawned,
 * since the logic under test (command_handler + the FreeRTOS task chain)
 * can't be faked without it -- captured via ADCS_LOG and checked for the
 * same per-task markers this test always checked.
 *
 * A prior bug (command_task_send() calling xQueueSend() from a
 * non-FreeRTOS-task pthread) let exactly the first command through and
 * then silently hung forever, which a "did this marker appear at all"
 * check would not have caught; these checks require a minimum number of
 * full round trips instead.
 *
 * ADCS_SIM_PATH is injected by tests/CMakeLists.txt via $<TARGET_FILE:...>,
 * so this always exercises the binary actually built by this build,
 * wherever the build directory lives.
 *
 * Run directly:
 *   ./build/tests/position_command_test
 * Or via CTest:
 *   ctest --test-dir build -R position_command_test --output-on-failure
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <pthread.h>

#include <csp/csp.h>
#include "csp_network.h"
#include "csp_commands.h"

#define CYCLES              6   /* commands sent, one every SEND_INTERVAL_SEC */
#define SEND_INTERVAL_SEC   2   /* > TELEMETRY_TASK_PERIOD_MS so each command gets its own telemetry report */
#define MIN_CYCLES          4   /* must survive well past the first 1-2 */
#define TEST_TIMEOUT_SEC    30

#define ADCS_LOG "/tmp/position_command_test_adcs.log"

/* Values stay far under fault_manager's POSITION_LIMIT (1000) -- this test
 * is about the happy path, not the reset/shutdown chain (see fallback.c /
 * health_monitor.c for that). */
#define BASE_TARGET_POSITION 100
#define TARGET_STEP          10

static int total_checks = 0;
static int failed_checks = 0;

static pthread_mutex_t telem_lock = PTHREAD_MUTEX_INITIALIZER;
static int telem_count = 0;
static int32_t last_telem_position = 0;

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

static int count_occurrences(const char * haystack, const char * marker) {
    if (haystack == NULL) {
        return 0;
    }

    int count = 0;
    const char * p = haystack;
    size_t marker_len = strlen(marker);
    while ((p = strstr(p, marker)) != NULL) {
        count++;
        p += marker_len;
    }
    return count;
}

/* Requires the marker to appear at least min_count times, not just once --
 * this is what actually catches a "works once then hangs" regression. */
static int check_min_count(const char * label, const char * haystack, const char * marker, int min_count) {
    total_checks++;
    int count = count_occurrences(haystack, marker);
    if (count >= min_count) {
        printf("[CHECK] PASS: %s (%d occurrences, need >= %d)\n", label, count, min_count);
        return count;
    }

    printf("[CHECK] FAIL: %s (only %d occurrences of \"%s\", need >= %d)\n", label, count, marker, min_count);
    failed_checks++;
    return count;
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

/* Stands in for what any real OBC role's telemetry listener would do:
 * bind ADCS_TELEM_PORT and record what ADCS reports back. Mirrors the
 * accept/read loop command_handler.c uses on the ADCS side. */
static void * telemetry_rx_loop(void * arg) {
    (void)arg;

    csp_socket_t sock = {0};
    if (csp_bind(&sock, ADCS_TELEM_PORT) != CSP_ERR_NONE) {
        fprintf(stderr, "[TEST] csp_bind on telemetry port failed\n");
        fflush(stderr);
        return NULL;
    }
    csp_listen(&sock, 5);

    while (1) {
        csp_conn_t * conn = csp_accept(&sock, 10000);
        if (conn == NULL) {
            continue; /* accept() timeout, try again */
        }

        csp_packet_t * packet;
        while ((packet = csp_read(conn, 50)) != NULL) {
            if (csp_conn_dport(conn) == ADCS_TELEM_PORT &&
                packet->length >= sizeof(position_telemetry_t)) {

                position_telemetry_t telem;
                memcpy(&telem, packet->data, sizeof(telem));

                pthread_mutex_lock(&telem_lock);
                telem_count++;
                last_telem_position = telem.current_position;
                pthread_mutex_unlock(&telem_lock);

                printf("[TEST] Telemetry: ADCS reports position=%d\n", telem.current_position);
                fflush(stdout);
            }

            csp_buffer_free(packet);
        }

        csp_close(conn);
    }

    return NULL;
}

int main(void) {
    alarm(TEST_TIMEOUT_SEC);

    /* Same hardcoded /tmp/comms_i2c.sock as production code -- don't run
     * this test at the same time as a real obc_supervisor/adcs_sim, or as
     * command_ack_test. */
    unlink("/tmp/comms_i2c.sock");
    unlink(ADCS_LOG);

    /* This test process *is* the OBC's CSP node -- master, address 1 --
     * same call any real OBC role makes (see apps/obc/commands/src/main.c). */
    csp_network_init(OBC_ADDRESS, /* is_master = */ 1);

    pthread_t telem_thread;
    if (pthread_create(&telem_thread, NULL, telemetry_rx_loop, NULL) != 0) {
        fprintf(stderr, "position_command_test: FAIL (could not start telemetry listener)\n");
        return 1;
    }

    pid_t adcs_pid = spawn_logged(ADCS_SIM_PATH, ADCS_LOG);
    if (adcs_pid < 0) {
        fprintf(stderr, "position_command_test: FAIL (could not spawn adcs_sim)\n");
        return 1;
    }

    usleep(500000); /* give ADCS a moment to come up and connect to the bus master */

    int acked = 0;
    for (int i = 0; i < CYCLES; i++) {
        int32_t target_position = BASE_TARGET_POSITION + i * TARGET_STEP;

        csp_conn_t * conn = csp_connect(CSP_PRIO_NORM, ADCS_ADDRESS, ADCS_CMD_PORT, 1000, CSP_O_NONE);
        if (conn == NULL) {
            printf("[TEST] cycle %d: failed to connect to ADCS\n", i);
            fflush(stdout);
            continue;
        }

        csp_packet_t * packet = csp_buffer_get(0);
        if (packet == NULL) {
            printf("[TEST] cycle %d: failed to get CSP buffer\n", i);
            fflush(stdout);
            csp_close(conn);
            continue;
        }

        position_command_t cmd = {
            .envelope = { .command_id = CMD_MOVE_TO_POSITION, .seq = (uint32_t)i },
            .target_position = target_position
        };
        memcpy(packet->data, &cmd, sizeof(cmd));
        packet->length = sizeof(cmd);
        csp_send(conn, packet);

        csp_packet_t * reply;
        while ((reply = csp_read(conn, 500)) != NULL) {
            if (reply->length >= sizeof(command_ack_t)) {
                command_ack_t ack;
                memcpy(&ack, reply->data, sizeof(ack));
                if (ack.ack_command_id == CMD_MOVE_TO_POSITION &&
                    ack.ack_seq == (uint32_t)i &&
                    ack.status == ACK) {
                    acked++;
                }
            }
            csp_buffer_free(reply);
        }
        csp_close(conn);

        printf("[TEST] cycle %d: sent position command target=%d\n", i, target_position);
        fflush(stdout);

        sleep(SEND_INTERVAL_SEC);
    }

    /* TELEMETRY_TASK_PERIOD_MS is 1000ms -- give the last command's report
     * time to arrive before we kill ADCS and grade the run. */
    usleep(1200000);

    kill(adcs_pid, SIGTERM);
    waitpid(adcs_pid, NULL, 0);

    char * adcs_log = slurp_file(ADCS_LOG);

    check_true("Test client received ACKs for its commands", acked >= MIN_CYCLES);

    pthread_mutex_lock(&telem_lock);
    int final_telem_count = telem_count;
    pthread_mutex_unlock(&telem_lock);
    check_true("Test client received telemetry reports", final_telem_count >= MIN_CYCLES - 1);

    /* ADCS side: command received and dispatched -- repeatedly */
    check_min_count("Command task dispatches to other tasks", adcs_log, "[COMMAND] Dispatching position command:", MIN_CYCLES);

    /* Every task the position command should activate -- repeatedly */
    check_min_count("Control task activated", adcs_log, "[CONTROL] Slewing toward target position:", MIN_CYCLES);
    check_min_count("Estimation task activated", adcs_log, "[ESTIMATION] Updating attitude estimate for target position:", MIN_CYCLES);
    check_min_count("Sensor task activated", adcs_log, "[SENSOR] Sampling sensors for move to position:", MIN_CYCLES);
    check_min_count("Telemetry task activated and reports back", adcs_log, "[TELEMETRY] Reporting new position to OBC:", MIN_CYCLES - 1);

    /* Housekeeping must NOT be activated by a position command */
    check_not_contains("Housekeeping stays silent", adcs_log, "HOUSEKEEPING");

    /* No local fault should have fired -- every target stayed well under
     * fault_manager's POSITION_LIMIT. A reset here would re-exec ADCS
     * mid-test and every check above would already be telling the story. */
    check_not_contains("ADCS never locally reset during normal traffic", adcs_log, "local fault, resetting");

    free(adcs_log);

    if (failed_checks == 0) {
        printf("position_command_test: PASS (%d/%d checks)\n", total_checks, total_checks);
        return 0;
    }

    fprintf(stderr, "position_command_test: FAIL (%d/%d checks failed)\n", failed_checks, total_checks);
    fprintf(stderr, "  See %s for full captured ADCS output.\n", ADCS_LOG);
    return 1;
}
