/*
 * Integration test for the full EPS command flow:
 *   test client -> CSP -> comms_bus -> EPS command_handler -> Command task
 *        -> manager / power rails
 *        -> Telemetry -> CSP -> comms_bus -> test client
 *
 * Mirrors test_position_command.c: this test process *is* the OBC's CSP node
 * (csp_network_init(OBC_ADDRESS, 1)) and the real eps_sim binary is spawned,
 * so it exercises the same wire contract without depending on which internal
 * OBC processes exist.
 *
 * EPS_SIM_PATH is injected by tests/CMakeLists.txt via $<TARGET_FILE:...>, so
 * this always exercises the binary actually built by this build.
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
#include "power_rail.h"

#define CYCLES              5
#define SEND_INTERVAL_SEC   1
#define MIN_CYCLES          3
#define TEST_TIMEOUT_SEC    30

#define EPS_LOG "/tmp/eps_command_test_eps.log"

/* eps_mode_t wire values (see apps/eps/include/communication/message.h). */
#define EPS_MODE_NOMINAL_WIRE 2

static int total_checks = 0;
static int failed_checks = 0;

static pthread_mutex_t telem_lock = PTHREAD_MUTEX_INITIALIZER;
static int telem_count = 0;
static int telem_magic_ok = 0;

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

static void * telemetry_rx_loop(void * arg) {
    (void)arg;

    csp_socket_t sock = {0};
    if (csp_bind(&sock, EPS_TELEM_PORT) != CSP_ERR_NONE) {
        fprintf(stderr, "[TEST] csp_bind on telemetry port failed\n");
        fflush(stderr);
        return NULL;
    }
    csp_listen(&sock, 5);

    while (1) {
        csp_conn_t * conn = csp_accept(&sock, 10000);
        if (conn == NULL) {
            continue;
        }

        csp_packet_t * packet;
        while ((packet = csp_read(conn, 50)) != NULL) {
            if (csp_conn_dport(conn) == EPS_TELEM_PORT && packet->length >= 4U) {
                pthread_mutex_lock(&telem_lock);
                telem_count++;
                if (packet->data[0] == 'E' && packet->data[1] == 'P' &&
                    packet->data[2] == 'S') {
                    telem_magic_ok++;
                }
                pthread_mutex_unlock(&telem_lock);

                printf("[TEST] Telemetry: EPS packet %u bytes\n",
                       (unsigned)packet->length);
                fflush(stdout);
            }

            csp_buffer_free(packet);
        }

        csp_close(conn);
    }

    return NULL;
}

/* Sends one command and returns the status the board reported for it: ACK,
   NACK, or -1 when no matching reply arrived. */
static int send_command_get_status(
    uint8_t command_id,
    uint32_t seq,
    const void * payload,
    size_t payload_length) {
    csp_conn_t * conn = csp_connect(CSP_PRIO_NORM, EPS_ADDRESS, EPS_CMD_PORT, 1000, CSP_O_NONE);
    if (conn == NULL) {
        return -1;
    }

    csp_packet_t * packet = csp_buffer_get(0);
    if (packet == NULL) {
        csp_close(conn);
        return -1;
    }

    memcpy(packet->data, payload, payload_length);
    packet->length = (uint16_t)payload_length;
    csp_send(conn, packet);

    int status = -1;
    csp_packet_t * reply;
    while ((reply = csp_read(conn, 500)) != NULL) {
        if (reply->length >= sizeof(command_ack_t)) {
            command_ack_t ack;
            memcpy(&ack, reply->data, sizeof(ack));
            if (ack.ack_command_id == command_id &&
                ack.ack_seq == seq) {
                status = (int)ack.status;
            }
        }
        csp_buffer_free(reply);
    }
    csp_close(conn);
    return status;
}

int main(void) {
    alarm(TEST_TIMEOUT_SEC);

    unlink("/tmp/comms_i2c.sock");
    unlink(EPS_LOG);

    csp_network_init(OBC_ADDRESS, /* is_master = */ 1);

    pthread_t telem_thread;
    if (pthread_create(&telem_thread, NULL, telemetry_rx_loop, NULL) != 0) {
        fprintf(stderr, "eps_command_test: FAIL (could not start telemetry listener)\n");
        return 1;
    }

    pid_t eps_pid = spawn_logged(EPS_SIM_PATH, EPS_LOG);
    if (eps_pid < 0) {
        fprintf(stderr, "eps_command_test: FAIL (could not spawn eps_sim)\n");
        return 1;
    }

    usleep(500000);

    int acked = 0;
    for (int i = 0; i < CYCLES; i++) {
        eps_mode_command_payload_t mode_cmd = {
            .envelope = { .command_id = EPS_WIRE_COMMAND_SET_MODE, .seq = (uint32_t)(i * 3 + 0) },
            .mode = EPS_MODE_NOMINAL_WIRE
        };
        if (send_command_get_status(EPS_WIRE_COMMAND_SET_MODE, (uint32_t)(i * 3 + 0), &mode_cmd, sizeof(mode_cmd)) == ACK) {
            acked++;
        }

        eps_rail_command_payload_t rail_cmd = {
            .envelope = { .command_id = EPS_WIRE_COMMAND_SET_RAIL, .seq = (uint32_t)(i * 3 + 1) },
            .rail_id = POWER_RAIL_PAYLOAD,
            .enabled = (uint8_t)(i % 2)
        };
        if (send_command_get_status(EPS_WIRE_COMMAND_SET_RAIL, (uint32_t)(i * 3 + 1), &rail_cmd, sizeof(rail_cmd)) == ACK) {
            acked++;
        }

        time_sync_command_t sync_cmd = {
            .envelope = { .command_id = CMD_TIME_SYNC, .seq = (uint32_t)(i * 3 + 2) },
            .unix_time_sec = 1700000000
        };
        if (send_command_get_status(CMD_TIME_SYNC, (uint32_t)(i * 3 + 2), &sync_cmd, sizeof(sync_cmd)) == ACK) {
            acked++;
        }

        printf("[TEST] cycle %d: sent mode/rail/time-sync commands\n", i);
        fflush(stdout);

        sleep(SEND_INTERVAL_SEC);
    }

    /* Boundary: malformed / out-of-range commands must be NACKed, and a valid
       command right after must still be ACKed -- proving bad input does not
       wedge the handler (same idea as test_command_ack.c). */
    eps_mode_command_payload_t bad_mode = {
        .envelope = { .command_id = EPS_WIRE_COMMAND_SET_MODE, .seq = 900 },
        .mode = 99
    };
    check_true("Out-of-range EPS mode is NACKed",
               send_command_get_status(EPS_WIRE_COMMAND_SET_MODE, 900, &bad_mode, sizeof(bad_mode)) == NACK);

    eps_rail_command_payload_t bad_rail = {
        .envelope = { .command_id = EPS_WIRE_COMMAND_SET_RAIL, .seq = 901 },
        .rail_id = 7,
        .enabled = 1
    };
    check_true("Out-of-range EPS rail id is NACKed",
               send_command_get_status(EPS_WIRE_COMMAND_SET_RAIL, 901, &bad_rail, sizeof(bad_rail)) == NACK);

    eps_mode_command_payload_t good_mode = {
        .envelope = { .command_id = EPS_WIRE_COMMAND_SET_MODE, .seq = 902 },
        .mode = EPS_MODE_NOMINAL_WIRE
    };
    check_true("Valid command after NACKs is ACKed",
               send_command_get_status(EPS_WIRE_COMMAND_SET_MODE, 902, &good_mode, sizeof(good_mode)) == ACK);

    usleep(1200000);

    kill(eps_pid, SIGTERM);
    waitpid(eps_pid, NULL, 0);

    char * eps_log = slurp_file(EPS_LOG);

    check_true("Test client received ACKs for its commands", acked >= MIN_CYCLES);

    pthread_mutex_lock(&telem_lock);
    int final_telem_count = telem_count;
    int final_magic_ok = telem_magic_ok;
    pthread_mutex_unlock(&telem_lock);
    check_true("Test client received EPS telemetry reports", final_telem_count >= MIN_CYCLES - 1);
    check_true("EPS telemetry starts with the EPS magic bytes", final_magic_ok >= MIN_CYCLES - 1);

    check_min_count("Command task applied mode changes", eps_log, "[COMMAND] Setting EPS mode to", MIN_CYCLES);
    check_min_count("Command task applied rail changes", eps_log, "[COMMAND] Rail ", MIN_CYCLES);
    check_min_count("Time sync command handled", eps_log, "[COMMAND HANDLER] Time sync command received.", MIN_CYCLES);

    free(eps_log);

    if (failed_checks == 0) {
        printf("eps_command_test: PASS (%d/%d checks)\n", total_checks, total_checks);
        return 0;
    }

    fprintf(stderr, "eps_command_test: FAIL (%d/%d checks failed)\n", failed_checks, total_checks);
    fprintf(stderr, "  See %s for full captured EPS output.\n", EPS_LOG);
    return 1;
}
