/*
 * Integration test for the ACK/NACK reply layered on top of the position
 * command envelope:
 *   OBC sends position_command_t (envelope + target) -> ADCS command_handler
 *   validates envelope.command_id -> replies command_ack_t on the same
 *   connection, before dispatching to the Command task -> OBC decodes the
 *   reply and logs ACK/NACK.
 *
 * Same approach as test_position_command.c: spawns the real obc_sim/adcs_sim
 * binaries, captures their stdout, and checks the log markers for this
 * feature specifically -- repeated ACKs matching the commands sent, and no
 * NACKs/buffer failures during normal (always-valid-command) traffic.
 *
 * OBC_SIM_PATH / ADCS_SIM_PATH are injected by tests/CMakeLists.txt via
 * $<TARGET_FILE:...>, so this always exercises the binaries actually built
 * by this build, wherever the build directory lives.
 *
 * The OBC's real send period is 30s; this test overrides it via
 * OBC_POSITION_PERIOD_SEC so it runs in seconds instead of minutes.
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

#define TEST_PERIOD_SEC     "2"
#define CAPTURE_SEC         13  /* long enough for ~6 command cycles at 2s */
#define TEST_TIMEOUT_SEC    25
#define MIN_CYCLES          4   /* must survive well past the first 1-2 */

#define OBC_LOG  "/tmp/command_ack_test_obc.log"
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
 * catches a "works once then stops" regression, not just "never worked". */
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

static pid_t spawn_logged(const char * path, const char * log_path, int set_period_env) {
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

        if (set_period_env) {
            setenv("OBC_POSITION_PERIOD_SEC", TEST_PERIOD_SEC, 1);
        }

        execl(path, path, (char *)NULL);
        perror("execl");
        _exit(127);
    }

    close(fd);
    return pid;
}

int main(void) {
    alarm(TEST_TIMEOUT_SEC);

    /* Same hardcoded /tmp/comms_i2c.sock as production code -- don't run
     * this test at the same time as a real obc_sim/adcs_sim, or as
     * position_command_test. */
    unlink("/tmp/comms_i2c.sock");
    unlink(OBC_LOG);
    unlink(ADCS_LOG);

    pid_t obc_pid = spawn_logged(OBC_SIM_PATH, OBC_LOG, 1);
    if (obc_pid < 0) {
        fprintf(stderr, "command_ack_test: FAIL (could not spawn obc_sim)\n");
        return 1;
    }

    usleep(500000); /* give the OBC (comms bus master) a head start on bind()/listen() */

    pid_t adcs_pid = spawn_logged(ADCS_SIM_PATH, ADCS_LOG, 0);
    if (adcs_pid < 0) {
        fprintf(stderr, "command_ack_test: FAIL (could not spawn adcs_sim)\n");
        kill(obc_pid, SIGKILL);
        waitpid(obc_pid, NULL, 0);
        return 1;
    }

    sleep(CAPTURE_SEC);

    kill(obc_pid, SIGTERM);
    kill(adcs_pid, SIGTERM);
    waitpid(obc_pid, NULL, 0);
    waitpid(adcs_pid, NULL, 0);

    char * obc_log = slurp_file(OBC_LOG);
    char * adcs_log = slurp_file(ADCS_LOG);

    /* OBC side: every command sent should get an ACK back, repeatedly. */
    int sent = check_min_count("OBC sends position command", obc_log, "[OBC] Sending position command:", MIN_CYCLES);
    int acked = check_min_count("OBC receives ACK for its commands", obc_log, "[OBC] Received ACK - Sequence:", MIN_CYCLES - 1);

    /* OBC only ever sends CMD_MOVE_TO_POSITION, which ADCS always
     * recognizes -- a NACK here would mean the envelope isn't being
     * populated/decoded correctly (e.g. the zero-init regression this
     * feature hit during development). */
    check_not_contains("No NACKs during normal traffic", obc_log, "[OBC] Received NACK - Sequence:");

    /* ADCS must be able to get a buffer for every reply it sends. */
    check_not_contains("ADCS never fails to get an ACK reply buffer", adcs_log, "Faiuled to get packet buffer");

    /* OBC must not be missing ACKs: it should have gotten one for
     * essentially every command it sent (allow 1 in flight at capture time). */
    total_checks++;
    if (sent > 0 && acked >= sent - 1) {
        printf("[CHECK] PASS: ACKs keep up with commands sent (sent=%d, acked=%d)\n", sent, acked);
    } else {
        printf("[CHECK] FAIL: OBC is missing ACKs (sent=%d, acked=%d)\n", sent, acked);
        failed_checks++;
    }

    free(obc_log);
    free(adcs_log);

    if (failed_checks == 0) {
        printf("command_ack_test: PASS (%d/%d checks)\n", total_checks, total_checks);
        return 0;
    }

    fprintf(stderr, "command_ack_test: FAIL (%d/%d checks failed)\n", failed_checks, total_checks);
    fprintf(stderr, "  See %s and %s for full captured output.\n", OBC_LOG, ADCS_LOG);
    return 1;
}
