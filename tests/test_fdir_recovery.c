#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define TEST_LOG "/tmp/fdir_recovery_test.log"
#define TEST_TIMEOUT_SEC 30
#define CHILD_LOOKUP_ATTEMPTS 50
#define RECOVERY_WAIT_ATTEMPTS 180
#define POLL_INTERVAL_US 100000

static volatile sig_atomic_t supervisor_pid = -1;
static int total_checks = 0;
static int failed_checks = 0;

/* Kill the isolated supervisor process group if the test itself times out. */
static void handle_alarm(int sig)
{
    (void)sig;

    if (supervisor_pid > 0) {
        kill(-(pid_t)supervisor_pid, SIGKILL);
    }
    _exit(124);
}

/*
 * Spawn Supervisor with stdout and stderr redirected into one log. Its
 * children inherit both descriptors, so their output is captured too.
 */
static pid_t spawn_supervisor(void)
{
    int fd = open(TEST_LOG, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open test log");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        close(fd);
        return -1;
    }

    if (pid == 0) {
        /* Isolate the constellation so timeout cleanup cannot hit the test. */
        if (setpgid(0, 0) != 0) {
            perror("setpgid");
            _exit(127);
        }

        if (dup2(fd, STDOUT_FILENO) < 0 || dup2(fd, STDERR_FILENO) < 0) {
            perror("dup2");
            _exit(127);
        }
        close(fd);

        execl(OBC_SUPERVISOR_PATH, OBC_SUPERVISOR_PATH, (char *)NULL);
        perror("execl obc_supervisor");
        _exit(127);
    }

    close(fd);

    /* The child also calls setpgid; this closes the small parent/child race. */
    if (setpgid(pid, pid) != 0 && errno != EACCES) {
        perror("setpgid");
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        return -1;
    }

    return pid;
}

static char *slurp_file(const char *path)
{
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    long size = ftell(file);
    if (size < 0) {
        fclose(file);
        return NULL;
    }

    rewind(file);

    char *contents = malloc((size_t)size + 1);
    if (contents == NULL) {
        fclose(file);
        return NULL;
    }

    size_t bytes_read = fread(contents, 1, (size_t)size, file);
    contents[bytes_read] = '\0';
    fclose(file);
    return contents;
}

static int count_occurrences(const char *text, const char *marker)
{
    int count = 0;
    size_t marker_length = strlen(marker);

    if (text == NULL || marker_length == 0) {
        return 0;
    }

    while ((text = strstr(text, marker)) != NULL) {
        count++;
        text += marker_length;
    }

    return count;
}

static int log_contains(const char *marker)
{
    char *log = slurp_file(TEST_LOG);
    int found = log != NULL && strstr(log, marker) != NULL;
    free(log);
    return found;
}

static int wait_for_log_marker(const char *marker, int attempts)
{
    for (int i = 0; i < attempts; i++) {
        if (log_contains(marker)) {
            return 1;
        }
        usleep(POLL_INTERVAL_US);
    }
    return 0;
}

/*
 * The Supervisor does not expose child PIDs through its protocol. For this
 * black-box test, ask the host process table for its obc_data child.
 */
static pid_t find_data_child(pid_t parent)
{
    char command[128];
    int written = snprintf(
        command,
        sizeof(command),
        "pgrep -P %ld -f '[/]obc_data$'",
        (long)parent
    );
    if (written < 0 || (size_t)written >= sizeof(command)) {
        return -1;
    }

    for (int i = 0; i < CHILD_LOOKUP_ATTEMPTS; i++) {
        FILE *pipe = popen(command, "r");
        if (pipe == NULL) {
            return -1;
        }

        long child = -1;
        int matched = fscanf(pipe, "%ld", &child);
        (void)pclose(pipe);

        if (matched == 1 && child > 0) {
            return (pid_t)child;
        }

        usleep(POLL_INTERVAL_US);
    }

    return -1;
}

static void check_contains(const char *label, const char *log, const char *marker)
{
    total_checks++;
    if (log != NULL && strstr(log, marker) != NULL) {
        printf("[CHECK] PASS: %s\n", label);
        return;
    }

    fprintf(stderr, "[CHECK] FAIL: %s (missing \"%s\")\n", label, marker);
    failed_checks++;
}

static void check_not_contains(const char *label, const char *log, const char *marker)
{
    total_checks++;
    if (log == NULL || strstr(log, marker) == NULL) {
        printf("[CHECK] PASS: %s\n", label);
        return;
    }

    fprintf(stderr, "[CHECK] FAIL: %s (unexpected \"%s\")\n", label, marker);
    failed_checks++;
}

static void check_exact_count(
    const char *label,
    const char *log,
    const char *marker,
    int expected
)
{
    total_checks++;
    int actual = count_occurrences(log, marker);
    if (actual == expected) {
        printf("[CHECK] PASS: %s\n", label);
        return;
    }

    fprintf(
        stderr,
        "[CHECK] FAIL: %s (expected %d occurrence(s), found %d)\n",
        label,
        expected,
        actual
    );
    failed_checks++;
}

static void check_in_order(
    const char *label,
    const char *log,
    const char *first,
    const char *second,
    const char *third,
    const char *fourth
)
{
    total_checks++;

    const char *p1 = log == NULL ? NULL : strstr(log, first);
    const char *p2 = p1 == NULL ? NULL : strstr(p1 + strlen(first), second);
    const char *p3 = p2 == NULL ? NULL : strstr(p2 + strlen(second), third);
    const char *p4 = p3 == NULL ? NULL : strstr(p3 + strlen(third), fourth);

    if (p4 != NULL) {
        printf("[CHECK] PASS: %s\n", label);
        return;
    }

    fprintf(stderr, "[CHECK] FAIL: %s (markers were missing or out of order)\n", label);
    failed_checks++;
}

int main(void)
{
    struct sigaction alarm_action = {0};
    alarm_action.sa_handler = handle_alarm;
    sigemptyset(&alarm_action.sa_mask);
    sigaction(SIGALRM, &alarm_action, NULL);
    alarm(TEST_TIMEOUT_SEC);

    unlink(TEST_LOG);
    unlink("/tmp/comms_i2c.sock");

    pid_t sup = spawn_supervisor();
    if (sup < 0) {
        fprintf(stderr, "fdir_recovery_test: FAIL (could not spawn Supervisor)\n");
        return 1;
    }
    supervisor_pid = (sig_atomic_t)sup;

    pid_t data = find_data_child(sup);
    if (data < 0) {
        fprintf(stderr, "fdir_recovery_test: FAIL (could not find Data child)\n");
        kill(-sup, SIGKILL);
        waitpid(sup, NULL, 0);
        return 1;
    }

    /* Let Data establish a healthy heartbeat baseline before freezing it. */
    sleep(3);

    if (kill(data, SIGSTOP) != 0) {
        perror("kill(SIGSTOP)");
        kill(-sup, SIGKILL);
        waitpid(sup, NULL, 0);
        return 1;
    }

    int recovered = wait_for_log_marker(
        "[FDIR WATCHDOG] data heartbeat recovered",
        RECOVERY_WAIT_ATTEMPTS
    );

    /* Always ask Supervisor to cleanly stop the constellation before checks. */
    kill(sup, SIGTERM);
    waitpid(sup, NULL, 0);
    supervisor_pid = -1;
    alarm(0);

    /* Remove any unexpected orphan before returning from a failed run. */
    kill(-sup, SIGKILL);

    char *log = slurp_file(TEST_LOG);
    if (!recovered) {
        fprintf(stderr, "[CHECK] FAIL: recovery did not complete before timeout\n");
        failed_checks++;
        total_checks++;
    } else {
        printf("[CHECK] PASS: recovery completed before timeout\n");
        total_checks++;
    }

    check_contains(
        "FDIR detected Data's missed deadline",
        log,
        "[FDIR WATCHDOG] data missed its heartbeat deadline"
    );
    check_contains(
        "FDIR requested Data's restart",
        log,
        "[SUPERVISOR SHUTDOWN] restarting data (requested by role 4)"
    );
    check_exact_count(
        "Supervisor restarted Data exactly once",
        log,
        "[OBC SUPERVISOR] Restarted data",
        1
    );
    check_contains(
        "FDIR observed Data's recovery",
        log,
        "[FDIR WATCHDOG] data heartbeat recovered"
    );
    check_in_order(
        "detection, request, restart, and recovery occurred in order",
        log,
        "[FDIR WATCHDOG] data missed its heartbeat deadline",
        "[SUPERVISOR SHUTDOWN] restarting data (requested by role 4)",
        "[OBC SUPERVISOR] Restarted data",
        "[FDIR WATCHDOG] data heartbeat recovered"
    );
    check_not_contains(
        "intentional restart was not classified as a crash",
        log,
        "Restarting data after crash"
    );
    check_not_contains("lifecycle operations did not race", log, "waitpid:");
    check_contains(
        "Supervisor shut down cleanly",
        log,
        "[OBC SUPERVISOR] Clean shutdown complete."
    );

    free(log);

    if (failed_checks == 0) {
        printf("fdir_recovery_test: PASS (%d/%d checks)\n", total_checks, total_checks);
        return 0;
    }

    fprintf(
        stderr,
        "fdir_recovery_test: FAIL (%d/%d checks failed); see %s\n",
        failed_checks,
        total_checks,
        TEST_LOG
    );
    return 1;
}
