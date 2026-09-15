/*
 * Full-constellation smoke test: spawns the real obc_supervisor (which
 * spawns fdir, commands, mission, time, data, and compute internally,
 * exactly like a real launch would) alongside a real adcs_sim, and checks
 * that the whole thing actually works together end to end -- not just that
 * each piece works in isolation, which is all the other tests here prove.
 *
 * Every other test in this suite either exercises one piece of the stack
 * directly (comms_bus_test, comms_bus_addressing_test) or stands in as a
 * bare CSP node to drive ADCS (position_command_test, command_ack_test).
 * None of them actually start supervisor and let it bring up the real
 * 7-process constellation the way a real launch would -- this is the one
 * that does, and it's the only test that would catch a regression in how
 * the processes talk to *each other* (a broken heartbeat causing a false
 * "frozen" restart, a role never getting spawned, IPC wiring between two
 * roles silently breaking) rather than a regression inside one process.
 *
 * Because obc_supervisor's children are spawned with posix_spawn and never
 * have their own stdout/stderr redirected, they inherit the supervisor
 * process's fds -- so redirecting *only* obc_supervisor's stdout/stderr to
 * a single log file captures every child's output too, interleaved. That's
 * exactly what this test relies on to check fdir/commands/mission/time/data
 * activity without needing five separate log files.
 *
 * MISSION_ASCENT_WAIT_SEC lets the real ~90 minute ascent wait run in a few
 * seconds instead, purely for this test -- production default is untouched
 * unless the env var is set (see scheduler.c).
 *
 * Run directly:
 *   ./build/tests/full_constellation_test
 * Or via CTest:
 *   ctest --test-dir build -R full_constellation_test --output-on-failure
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>

#define ASCENT_OVERRIDE_SEC     "2"
#define TIME_SYNC_OVERRIDE_SEC  "1"
#define CAPTURE_SEC             10
#define TEST_TIMEOUT_SEC        30

#define SUPERVISOR_LOG "/tmp/full_constellation_test_supervisor.log"
#define ADCS_LOG       "/tmp/full_constellation_test_adcs.log"

static int total_checks = 0;
static int failed_checks = 0;

static char * slurp_file(const char * path) {
    FILE * f = fopen(path, "r");
    if (f == NULL) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) { fclose(f); return NULL; }

    char * buf = malloc((size_t)size + 1);
    if (buf == NULL) { fclose(f); return NULL; }

    size_t read_bytes = fread(buf, 1, (size_t)size, f);
    buf[read_bytes] = '\0';
    fclose(f);
    return buf;
}

static void check_contains(const char * label, const char * haystack, const char * marker) {
    total_checks++;
    if (haystack != NULL && strstr(haystack, marker) != NULL) {
        printf("[CHECK] PASS: %s\n", label);
    } else {
        printf("[CHECK] FAIL: %s (did not find \"%s\")\n", label, marker);
        failed_checks++;
    }
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

static pid_t spawn_logged(const char * path, const char * log_path, int set_ascent_override) {
    int fd = open(log_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { perror("open"); return -1; }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); close(fd); return -1; }

    if (pid == 0) {
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);

        if (set_ascent_override) {
            setenv("MISSION_ASCENT_WAIT_SEC", ASCENT_OVERRIDE_SEC, 1);
            setenv("TIME_SYNC_INTERVAL_SEC", TIME_SYNC_OVERRIDE_SEC, 1);
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

    /* Same hardcoded sockets as production code -- don't run this test
     * alongside a manually-launched constellation or any other test that
     * touches /tmp/comms_i2c.sock or the obc_ipc_*.sock files. */
    unlink("/tmp/comms_i2c.sock");
    unlink(SUPERVISOR_LOG);
    unlink(ADCS_LOG);

    pid_t sup_pid = spawn_logged(OBC_SUPERVISOR_PATH, SUPERVISOR_LOG, 1);
    if (sup_pid < 0) {
        fprintf(stderr, "full_constellation_test: FAIL (could not spawn obc_supervisor)\n");
        return 1;
    }

    usleep(500000); /* give supervisor + its children a head start */

    pid_t adcs_pid = spawn_logged(ADCS_SIM_PATH, ADCS_LOG, 0);
    if (adcs_pid < 0) {
        fprintf(stderr, "full_constellation_test: FAIL (could not spawn adcs_sim)\n");
        kill(sup_pid, SIGKILL);
        waitpid(sup_pid, NULL, 0);
        return 1;
    }

    sleep(CAPTURE_SEC);

    kill(sup_pid, SIGTERM);
    kill(adcs_pid, SIGTERM);
    waitpid(sup_pid, NULL, 0);
    waitpid(adcs_pid, NULL, 0);

    char * sup_log = slurp_file(SUPERVISOR_LOG);
    char * adcs_log = slurp_file(ADCS_LOG);

    /* Every real role actually started (compute is intentionally excluded --
       it's still a stub that exits immediately, see the exit-code check below). */
    check_contains("supervisor started all processes", sup_log, "[SUPERVISOR] Successfully started all processes.");
    check_contains("fdir came up", sup_log, "[HEARTBEAT] Attempting Thread Init.");
    check_contains("commands came up", sup_log, "[INGEST THREAD] Init Successful.");
    check_contains("mission came up", sup_log, "[OBC MISSION] Initializing.");
    check_contains("time came up", sup_log, "[OBC TIME] Initializing.");
    check_contains("data came up", sup_log, "[OBC DATA] Initializing.");
    check_contains("compute came up", sup_log, "[OBC COMPUTE] Initializing.");

    /* autonomy and time_sync both fire on their very first tick (their
       last-fired baseline starts at zero), so both should reach ADCS well
       within the capture window without waiting out their real intervals. */
    check_contains("autonomy's point-to-sun reached ADCS", adcs_log, "[COMMAND HANDLER] Point to sun command received.");
    check_contains("time's sync reached ADCS", adcs_log, "[COMMAND HANDLER] Time sync command received.");

    /* mission's full scripted timeline, sped up via MISSION_ASCENT_WAIT_SEC:
       ascent -> photo -> compute compresses it -> data streams it back -> downlink. */
    check_contains("mission reached the ascent window", sup_log, "[SCHEDULER] Ascent window reached");
    check_contains("mission requested a photo capture", sup_log, "[PAYLOAD COMMANDER] Requesting photo capture");
    check_contains("camera captured the mock photo", sup_log, "[CAMERA] (mock) captured photo");
    check_contains("mission requested compression of the photo", sup_log, "[PAYLOAD COMMANDER] Requesting compression of");
    check_contains("compute finished the compression job", sup_log, "[PAYLOAD COMMANDER] Compression done:");
    check_contains("mission asked data to stream the (compressed) photo back", sup_log, "[STORAGE] Streaming");
    check_contains("radio downlink fired with the retrieved bytes", sup_log, "[RADIO] (mock) would transmit");

    /* The bug class this whole test suite exists to catch: a real, healthy
       process getting killed and restarted because its heartbeat wasn't
       wired up. All 6 non-supervisor roles are real now -- none of them
       should show up in an exit/kill message during the run. */
    check_not_contains("supervisor never falsely restarted a healthy process", sup_log, "appears frozen");
    check_not_contains("fdir never exited/was killed", sup_log, "fdir exited");
    check_not_contains("commands never exited/was killed", sup_log, "commands exited");
    check_not_contains("mission never exited/was killed", sup_log, "mission exited");
    check_not_contains("time never exited/was killed", sup_log, "time exited");
    check_not_contains("data never exited/was killed", sup_log, "data exited");
    check_not_contains("compute never exited/was killed", sup_log, "compute exited");

    check_contains("supervisor shut down cleanly on SIGTERM", sup_log, "[OBC SUPERVISOR] Clean shutdown complete.");

    free(sup_log);
    free(adcs_log);

    if (failed_checks == 0) {
        printf("full_constellation_test: PASS (%d/%d checks)\n", total_checks, total_checks);
        return 0;
    }

    fprintf(stderr, "full_constellation_test: FAIL (%d/%d checks failed)\n", failed_checks, total_checks);
    fprintf(stderr, "  See %s and %s for full captured output.\n", SUPERVISOR_LOG, ADCS_LOG);
    return 1;
}
