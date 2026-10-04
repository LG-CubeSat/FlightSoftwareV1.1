#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "obc_ipc.h"
#include "obc_telemetry_protocol.h"

#define TEST_LOG_PATH "/tmp/data_telemetry_test.bin"
#define TEST_TIMEOUT_SEC 15
#define RETRY_COUNT 100
#define RETRY_DELAY_US 20000

static volatile sig_atomic_t data_pid = -1;
static int total_checks = 0;
static int failed_checks = 0;

static void check_true(const char *label, int condition)
{
    total_checks++;

    if (condition) {
        printf("[PASS] %s\n", label);
    } else {
        fprintf(stderr, "[FAIL] %s\n", label);
        failed_checks++;
    }
}

static void handle_alarm(int sig)
{
    (void)sig;

    if (data_pid > 0) {
        kill((pid_t)data_pid, SIGKILL);
    }

    _exit(124);
}

static pid_t start_data(const char *data_path)
{
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        execl(data_path, data_path, (char *)NULL);
        perror("execl obc_data");
        _exit(127);
    }

    data_pid = (sig_atomic_t)pid;
    return pid;
}

static void stop_data(pid_t pid)
{
    if (pid <= 0) {
        return;
    }

    kill(pid, SIGTERM);
    waitpid(pid, NULL, 0);
    data_pid = -1;
}

static int send_with_retry(
    const uint8_t *encoded,
    size_t encoded_size
)
{
    for (int attempt = 0; attempt < RETRY_COUNT; attempt++) {
        int result = IPC_send(
            ROLE_DATA,
            encoded,
            (uint16_t)encoded_size
        );

        if (result >= 0) {
            return 0;
        }

        usleep(RETRY_DELAY_US);
    }

    return -1;
}

static int wait_for_file_size(size_t expected)
{
    for (int attempt = 0; attempt < RETRY_COUNT; attempt++) {
        struct stat info;

        if (
            stat(TEST_LOG_PATH, &info) == 0 &&
            (size_t)info.st_size == expected
        ) {
            return 1;
        }

        usleep(RETRY_DELAY_US);
    }

    return 0;
}

static int encode_test_record(
    uint64_t timestamp,
    uint8_t payload_value,
    uint8_t *encoded,
    size_t *encoded_size
)
{
    obc_telemetry_record_t record = {
        .source_node = 2,
        .source_port = 20,
        .received_unix_us = timestamp,
        .payload_length = 3,
        .payload = {
            payload_value,
            payload_value,
            payload_value
        }
    };

    return obc_telemetry_encode(
        &record,
        encoded,
        OBC_IPC_MAX_PAYLOAD,
        encoded_size
    );
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(
            stderr,
            "usage: %s /path/to/obc_data\n",
            argv[0]
        );
        return 1;
    }

    struct sigaction alarm_action = {0};
    alarm_action.sa_handler = handle_alarm;
    sigemptyset(&alarm_action.sa_mask);
    sigaction(SIGALRM, &alarm_action, NULL);
    alarm(TEST_TIMEOUT_SEC);

    unlink(TEST_LOG_PATH);
    unlink("/tmp/obc_ipc_data.sock");
    unlink("/tmp/obc_ipc_commands.sock");

    setenv(
        "OBC_TELEMETRY_LOG_PATH",
        TEST_LOG_PATH,
        1
    );

    /*
     * Data accepts telemetry only from ROLE_COMMANDS, so the test acts as
     * Commands for the duration of this process.
     */
    if (IPC_initialize(ROLE_COMMANDS) != IPC_OK) {
        fprintf(stderr, "could not initialize Commands IPC\n");
        return 1;
    }

    uint8_t first[OBC_IPC_MAX_PAYLOAD];
    uint8_t second[OBC_IPC_MAX_PAYLOAD];
    uint8_t third[OBC_IPC_MAX_PAYLOAD];

    size_t first_size = 0;
    size_t second_size = 0;
    size_t third_size = 0;

    check_true(
        "first record encodes",
        encode_test_record(
            1000,
            0x11,
            first,
            &first_size
        ) == OBC_TELEMETRY_OK
    );

    check_true(
        "second record encodes",
        encode_test_record(
            2000,
            0x22,
            second,
            &second_size
        ) == OBC_TELEMETRY_OK
    );

    check_true(
        "third record encodes",
        encode_test_record(
            3000,
            0x33,
            third,
            &third_size
        ) == OBC_TELEMETRY_OK
    );

    pid_t first_data = start_data(argv[1]);
    if (first_data < 0) {
        return 1;
    }

    check_true(
        "first record reaches Data",
        send_with_retry(first, first_size) == 0
    );

    check_true(
        "second record reaches Data",
        send_with_retry(second, second_size) == 0
    );

    size_t first_run_size = first_size + second_size;

    check_true(
        "first two records are persisted",
        wait_for_file_size(first_run_size)
    );

    /*
     * Corrupt the magic. Data should reject this without changing the file.
     */
    uint8_t malformed[OBC_IPC_MAX_PAYLOAD];
    memcpy(malformed, first, first_size);
    malformed[0] ^= 0xFF;

    check_true(
        "malformed record reaches Data for validation",
        send_with_retry(malformed, first_size) == 0
    );

    usleep(200000);

    check_true(
        "malformed record is not appended",
        wait_for_file_size(first_run_size)
    );

    stop_data(first_data);

    /*
     * Starting a fresh Data process simulates recovery or an OBC restart.
     * Opening with "ab" must preserve the first two records.
     */
    pid_t second_data = start_data(argv[1]);
    if (second_data < 0) {
        return 1;
    }

    check_true(
        "third record reaches restarted Data",
        send_with_retry(third, third_size) == 0
    );

    size_t final_size =
        first_size + second_size + third_size;

    check_true(
        "restarted Data appends instead of truncating",
        wait_for_file_size(final_size)
    );

    stop_data(second_data);
    alarm(0);

    FILE *file = fopen(TEST_LOG_PATH, "rb");
    check_true("telemetry log can be reopened", file != NULL);

    uint8_t *contents = NULL;

    if (file != NULL) {
        contents = malloc(final_size);

        check_true(
            "telemetry log buffer allocated",
            contents != NULL
        );

        if (contents != NULL) {
            size_t bytes_read = fread(
                contents,
                1,
                final_size,
                file
            );

            check_true(
                "complete telemetry log can be read",
                bytes_read == final_size
            );

            check_true(
                "first record bytes are preserved",
                memcmp(contents, first, first_size) == 0
            );

            check_true(
                "second record bytes are preserved",
                memcmp(
                    contents + first_size,
                    second,
                    second_size
                ) == 0
            );

            check_true(
                "third record bytes are appended",
                memcmp(
                    contents + first_size + second_size,
                    third,
                    third_size
                ) == 0
            );
        }

        fclose(file);
    }

    free(contents);

    if (failed_checks == 0) {
        printf(
            "data_telemetry_test: PASS (%d/%d checks)\n",
            total_checks,
            total_checks
        );
        return 0;
    }

    fprintf(
        stderr,
        "data_telemetry_test: FAIL (%d/%d checks failed)\n",
        failed_checks,
        total_checks
    );
    return 1;
}