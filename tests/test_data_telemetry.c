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
    uint16_t payload_length,
    uint8_t *encoded,
    size_t *encoded_size
)
{
    if (payload_length > OBC_TELEMETRY_MAX_PAYLOAD) {
        return OBC_TELEMETRY_INVALID_ARGUMENT;
    }

    obc_telemetry_record_t record = {
        .source_node = 2,
        .source_port = 20,
        .received_unix_us = timestamp,
        .payload_length = payload_length
    };

    memset(
        record.payload,
        payload_value,
        payload_length
    );

    return obc_telemetry_encode(
        &record,
        encoded,
        OBC_IPC_MAX_PAYLOAD,
        encoded_size
    );
}

typedef struct {
    obc_telemetry_read_status_t status;
    uint8_t encoded[OBC_IPC_MAX_PAYLOAD];
    size_t encoded_size;
    uint64_t next_offset;
    int end_of_log;
    int chunk_count;
} telemetry_read_result_t;

static int request_telemetry_record(
    uint64_t requested_offset,
    telemetry_read_result_t *result
)
{
    if (result == NULL) {
        return -1;
    }

    memset(result, 0, sizeof(*result));
    result->status = OBC_TELEMETRY_READ_IO_ERROR;

    obc_telemetry_read_request_t request = {
        .magic = OBC_TELEMETRY_READ_REQUEST_MAGIC,
        .offset = requested_offset
    };

    if (
        send_with_retry(
            (const uint8_t *)&request,
            sizeof(request)
        ) != 0
    ) {
        return -1;
    }

    /*
     * A 256-byte record requires at most two 216-byte chunks. Allow four
     * iterations so malformed behavior fails cleanly instead of looping.
     */
    for (int reply_number = 0; reply_number < 4; reply_number++) {
        OBC_Roles_t source;
        obc_telemetry_read_reply_t reply;

        int length = IPC_receive_timeout(
            &source,
            (uint8_t *)&reply,
            sizeof(reply),
            1000
        );

        if (
            length != (int)sizeof(reply) ||
            source != ROLE_DATA ||
            reply.magic != OBC_TELEMETRY_READ_REPLY_MAGIC
        ) {
            return -1;
        }

        result->status =
            (obc_telemetry_read_status_t)reply.status;

        if (result->status != OBC_TELEMETRY_READ_OK) {
            if (
                !reply.is_last_chunk ||
                reply.chunk_length != 0
            ) {
                return -1;
            }

            result->next_offset = reply.next_offset;
            result->end_of_log = reply.end_of_log;
            return 0;
        }

        if (
            reply.record_offset != requested_offset ||
            reply.record_length > sizeof(result->encoded) ||
            reply.chunk_length > sizeof(reply.payload) ||
            reply.chunk_offset != result->encoded_size ||
            (size_t)reply.chunk_offset + reply.chunk_length >
                reply.record_length
        ) {
            return -1;
        }

        memcpy(
            result->encoded + reply.chunk_offset,
            reply.payload,
            reply.chunk_length
        );

        result->encoded_size += reply.chunk_length;
        result->chunk_count++;

        if (reply.is_last_chunk) {
            if (result->encoded_size != reply.record_length) {
                return -1;
            }

            result->next_offset = reply.next_offset;
            result->end_of_log = reply.end_of_log;
            return 0;
        }
    }

    return -1;
}

int main(int argc, char **argv)
{
    unlink("/tmp/obc_ipc_mission.sock");
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
            3,
            first,
            &first_size
        ) == OBC_TELEMETRY_OK
    );

    check_true(
        "second record encodes",
        encode_test_record(
            2000,
            0x22,
            3,
            second,
            &second_size
        ) == OBC_TELEMETRY_OK
    );

    check_true(
        "third record encodes",
        encode_test_record(
            3000,
            0x33,
            OBC_TELEMETRY_MAX_PAYLOAD,
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

    /* Record insertion is complete. Rebind the test as Mission so Data
       authorizes retrieval and has a Mission socket for its replies. */
    check_true(
        "test switches to Mission IPC",
        IPC_initialize(ROLE_MISSION) == IPC_OK
    );

    telemetry_read_result_t first_read;
    telemetry_read_result_t second_read;
    telemetry_read_result_t third_read;
    telemetry_read_result_t end_read;

    check_true(
        "first record can be requested",
        request_telemetry_record(0, &first_read) == 0
    );
    check_true(
        "first record matches stored bytes",
        first_read.status == OBC_TELEMETRY_READ_OK &&
        first_read.encoded_size == first_size &&
        memcmp(first_read.encoded, first, first_size) == 0
    );
    check_true(
        "first cursor points to second record",
        first_read.next_offset == first_size &&
        !first_read.end_of_log
    );

    check_true(
        "second record can be requested",
        request_telemetry_record(first_read.next_offset, &second_read) == 0
    );
    check_true(
        "second record matches stored bytes",
        second_read.status == OBC_TELEMETRY_READ_OK &&
        second_read.encoded_size == second_size &&
        memcmp(second_read.encoded, second, second_size) == 0
    );
    check_true(
        "second cursor points to third record",
        second_read.next_offset == first_size + second_size &&
        !second_read.end_of_log
    );

    check_true(
        "third record can be requested",
        request_telemetry_record(second_read.next_offset, &third_read) == 0
    );
    check_true(
        "maximum record is reconstructed exactly",
        third_read.status == OBC_TELEMETRY_READ_OK &&
        third_read.encoded_size == third_size &&
        memcmp(third_read.encoded, third, third_size) == 0
    );
    check_true(
        "maximum record required two IPC chunks",
        third_read.chunk_count == 2
    );
    check_true(
        "third record reaches the current end of log",
        third_read.end_of_log &&
        third_read.next_offset == final_size
    );

    check_true(
        "request beyond final record receives END",
        request_telemetry_record(third_read.next_offset, &end_read) == 0 &&
        end_read.status == OBC_TELEMETRY_READ_END &&
        end_read.end_of_log &&
        end_read.encoded_size == 0
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
