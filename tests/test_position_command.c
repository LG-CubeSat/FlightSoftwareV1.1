/*
 * Integration test for the full position-command flow:
 *   OBC -> CSP -> comms_bus -> ADCS command_handler -> Command task
 *        -> (Control, Estimation, Sensor, Telemetry tasks; NOT Housekeeping)
 *        -> Telemetry -> CSP -> comms_bus -> OBC
 *
 * Unlike test_comms_bus.c, this doesn't link the code under test directly --
 * the logic under test is spread across the full FreeRTOS scheduler inside
 * adcs_sim, not a couple of synchronous functions. Instead this spawns the
 * real compiled obc_sim/adcs_sim binaries as child processes, captures
 * their stdout, and checks that every expected marker in the chain above
 * appears -- repeatedly, not just once. A prior bug (command_task_send()
 * calling xQueueSend() from a non-FreeRTOS-task pthread) let exactly the
 * first command through and then silently hung forever, which a "did this
 * marker appear at all" check would not have caught; these checks require
 * a minimum number of full round trips instead.
 *
 * OBC_SIM_PATH / ADCS_SIM_PATH are injected by tests/CMakeLists.txt via
 * $<TARGET_FILE:...>, so this always exercises the binaries actually built
 * by this build, wherever the build directory lives.
 *
 * The OBC's real send period is 30s; this test overrides it via
 * OBC_POSITION_PERIOD_SEC so it runs in seconds instead of minutes,
 * without touching the production default.
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

#define TEST_PERIOD_SEC     "2"
#define CAPTURE_SEC         13  /* long enough for ~6 command cycles at 2s */
#define TEST_TIMEOUT_SEC    25
#define MIN_CYCLES          4   /* must survive well past the first 1-2 */

#define OBC_LOG  "/tmp/position_command_test_obc.log"
#define ADCS_LOG "/tmp/position_command_test_adcs.log"

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

/* Requires the marker to appear at least MIN_CYCLES times, not just once --
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
     * this test at the same time as a real obc_sim/adcs_sim. */
    unlink("/tmp/comms_i2c.sock");
    unlink(OBC_LOG);
    unlink(ADCS_LOG);

    pid_t obc_pid = spawn_logged(OBC_SIM_PATH, OBC_LOG, 1);
    if (obc_pid < 0) {
        fprintf(stderr, "position_command_test: FAIL (could not spawn obc_sim)\n");
        return 1;
    }

    usleep(500000); /* give the OBC (comms bus master) a head start on bind()/listen() */

    pid_t adcs_pid = spawn_logged(ADCS_SIM_PATH, ADCS_LOG, 0);
    if (adcs_pid < 0) {
        fprintf(stderr, "position_command_test: FAIL (could not spawn adcs_sim)\n");
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

    /* OBC side: command sent, telemetry reply received -- repeatedly */
    int sent = check_min_count("OBC sends position command", obc_log, "[OBC] Sending position command:", MIN_CYCLES);
    check_min_count("OBC receives telemetry reply", obc_log, "[OBC] Telemetry: ADCS reports position=", MIN_CYCLES - 1);

    /* ADCS side: command received and dispatched -- repeatedly */
    int received = check_min_count("ADCS command_handler decodes command", adcs_log, "[COMMAND HANDLER] Position command received:", MIN_CYCLES);
    check_min_count("Command task dispatches to other tasks", adcs_log, "[COMMAND] Dispatching position command:", MIN_CYCLES);

    /* Every task the position command should activate -- repeatedly */
    check_min_count("Control task activated", adcs_log, "[CONTROL] Slewing toward target position:", MIN_CYCLES);
    check_min_count("Estimation task activated", adcs_log, "[ESTIMATION] Updating attitude estimate for target position:", MIN_CYCLES);
    check_min_count("Sensor task activated", adcs_log, "[SENSOR] Sampling sensors for move to position:", MIN_CYCLES);
    check_min_count("Telemetry task activated and reports back", adcs_log, "[TELEMETRY] Reporting new position to OBC:", MIN_CYCLES - 1);

    /* ADCS must not be silently dropping commands: it should have received
     * essentially everything OBC sent (allow 1 in flight at capture time). */
    total_checks++;
    if (sent > 0 && received >= sent - 1) {
        printf("[CHECK] PASS: ADCS keeps up with OBC (sent=%d, received=%d)\n", sent, received);
    } else {
        printf("[CHECK] FAIL: ADCS is losing commands (sent=%d, received=%d)\n", sent, received);
        failed_checks++;
    }

    /* Housekeeping must NOT be activated by a position command */
    check_not_contains("Housekeeping stays silent", adcs_log, "HOUSEKEEPING");

    free(obc_log);
    free(adcs_log);

    if (failed_checks == 0) {
        printf("position_command_test: PASS (%d/%d checks)\n", total_checks, total_checks);
        return 0;
    }

    fprintf(stderr, "position_command_test: FAIL (%d/%d checks failed)\n", failed_checks, total_checks);
    fprintf(stderr, "  See %s and %s for full captured output.\n", OBC_LOG, ADCS_LOG);
    return 1;
}
