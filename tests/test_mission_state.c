#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define STATE_PATH "/tmp/mission_state_test.state"
#define STATE_TEMP_PATH STATE_PATH ".tmp"
#define LOG_PATH "/tmp/mission_state_test.log"
#define MISSION_SOCKET "/tmp/obc_ipc_mission.sock"
#define WAIT_STEP_US 20000
#define WAIT_TIMEOUT_MS 3000

static int checks_failed;
static pid_t active_mission = -1;

static void check_true(const char *name, int condition)
{
    if (condition) {
        printf("[PASS] %s\n", name);
        return;
    }

    fprintf(stderr, "[FAIL] %s\n", name);
    checks_failed++;
}

static void cleanup_files(void)
{
    unlink(STATE_PATH);
    unlink(STATE_TEMP_PATH);
    unlink(LOG_PATH);
    unlink(MISSION_SOCKET);
}

static void stop_mission(void)
{
    if (active_mission <= 0) {
        return;
    }

    kill(active_mission, SIGTERM);
    waitpid(active_mission, NULL, 0);
    active_mission = -1;
    unlink(MISSION_SOCKET);
}

static int file_contains(const char *path, const char *needle)
{
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    char line[512];
    int found = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        if (strstr(line, needle) != NULL) {
            found = 1;
            break;
        }
    }

    fclose(file);
    return found;
}

static int wait_for_log(const char *needle)
{
    int waited_ms = 0;
    while (waited_ms < WAIT_TIMEOUT_MS) {
        if (file_contains(LOG_PATH, needle)) {
            return 1;
        }

        usleep(WAIT_STEP_US);
        waited_ms += WAIT_STEP_US / 1000;
    }

    return 0;
}

static int write_state(
    int phase,
    long long mission_start,
    int include_cursor,
    uint64_t cursor
)
{
    FILE *file = fopen(STATE_PATH, "w");
    if (file == NULL) {
        return -1;
    }

    int result;
    if (include_cursor) {
        result = fprintf(
            file,
            "%d %lld %" PRIu64 "\n",
            phase,
            mission_start,
            cursor
        );
    } else {
        result = fprintf(file, "%d %lld\n", phase, mission_start);
    }

    if (fclose(file) != 0) {
        return -1;
    }

    return result < 0 ? -1 : 0;
}

static pid_t spawn_mission(const char *mission_path)
{
    unlink(MISSION_SOCKET);

    int log_fd = open(LOG_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (log_fd < 0) {
        perror("open mission state test log");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork obc_mission");
        close(log_fd);
        return -1;
    }

    if (pid == 0) {
        dup2(log_fd, STDOUT_FILENO);
        dup2(log_fd, STDERR_FILENO);
        close(log_fd);

        setenv("MISSION_STATE_PATH", STATE_PATH, 1);
        setenv("MISSION_ASCENT_WAIT_SEC", "3600", 1);

        execl(mission_path, mission_path, (char *)NULL);
        perror("execl obc_mission");
        _exit(127);
    }

    close(log_fd);
    return pid;
}

static void test_legacy_state(const char *mission_path)
{
    cleanup_files();
    long long now = (long long)time(NULL);

    check_true(
        "legacy state fixture is written",
        write_state(0, now, 0, 0) == 0
    );

    active_mission = spawn_mission(mission_path);
    check_true("Mission starts for legacy state", active_mission > 0);
    if (active_mission <= 0) {
        return;
    }

    check_true(
        "legacy two-field state resumes at cursor zero",
        wait_for_log("resumed mission: phase=0") &&
        file_contains(LOG_PATH, "telemetry_cursor=0")
    );
    stop_mission();
}

static void test_new_state(const char *mission_path)
{
    cleanup_files();
    long long now = (long long)time(NULL);

    check_true(
        "new state fixture is written",
        write_state(0, now, 1, UINT64_C(338)) == 0
    );

    active_mission = spawn_mission(mission_path);
    check_true("Mission starts for new state", active_mission > 0);
    if (active_mission <= 0) {
        return;
    }

    check_true(
        "three-field state restores its telemetry cursor",
        wait_for_log("resumed mission: phase=0") &&
        file_contains(LOG_PATH, "telemetry_cursor=338")
    );
    stop_mission();
}

static void test_fresh_state(const char *mission_path)
{
    cleanup_files();

    active_mission = spawn_mission(mission_path);
    check_true("Mission starts without a state file", active_mission > 0);
    if (active_mission <= 0) {
        return;
    }

    check_true(
        "Mission reports a fresh start",
        wait_for_log("[SCHEDULER] fresh mission")
    );

    int phase = -1;
    long long mission_start = 0;
    uint64_t cursor = UINT64_MAX;
    int parsed = 0;

    FILE *file = fopen(STATE_PATH, "r");
    if (file != NULL) {
        parsed = fscanf(
            file,
            "%d %lld %" SCNu64,
            &phase,
            &mission_start,
            &cursor
        );
        fclose(file);
    }

    check_true(
        "fresh Mission writes a three-field state file",
        parsed == 3
    );
    check_true(
        "fresh Mission initializes phase, time, and cursor",
        phase == 0 && mission_start > 0 && cursor == 0
    );
    stop_mission();
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s /path/to/obc_mission\n", argv[0]);
        return 2;
    }

    alarm(15);
    test_legacy_state(argv[1]);
    test_new_state(argv[1]);
    test_fresh_state(argv[1]);
    stop_mission();
    cleanup_files();
    alarm(0);

    if (checks_failed == 0) {
        printf("mission_state_test: PASS\n");
        return 0;
    }

    fprintf(
        stderr,
        "mission_state_test: FAIL (%d checks failed)\n",
        checks_failed
    );
    return 1;
}
