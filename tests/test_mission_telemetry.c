#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mission_health.h"
#include "obc_ipc.h"
#include "obc_telemetry_protocol.h"
#include "payload_commander.h"

#define MAX_REPLIES 8
#define MAX_RADIO_CALLS 8

static int checks_failed;
static int reply_count;
static int next_reply;
static obc_telemetry_read_reply_t replies[MAX_REPLIES];

static int request_count;
static obc_telemetry_read_request_t last_request;
static obc_telemetry_read_request_t requests[MAX_REPLIES];

static int radio_result;
static int radio_fail_on_call;
static int radio_calls;
static uint8_t radio_bytes[OBC_IPC_MAX_PAYLOAD];
static size_t radio_length;
static uint8_t radio_history[MAX_RADIO_CALLS][OBC_IPC_MAX_PAYLOAD];
static size_t radio_history_lengths[MAX_RADIO_CALLS];
static int progress_calls;

static void check_true(const char *name, int condition)
{
    if (condition) {
        printf("[PASS] %s\n", name);
        return;
    }

    fprintf(stderr, "[FAIL] %s\n", name);
    checks_failed++;
}

static void reset_fakes(void)
{
    reply_count = 0;
    next_reply = 0;
    memset(replies, 0, sizeof(replies));
    request_count = 0;
    memset(&last_request, 0, sizeof(last_request));
    memset(requests, 0, sizeof(requests));
    radio_result = 0;
    radio_fail_on_call = 0;
    radio_calls = 0;
    radio_length = 0;
    memset(radio_bytes, 0, sizeof(radio_bytes));
    memset(radio_history, 0, sizeof(radio_history));
    memset(radio_history_lengths, 0, sizeof(radio_history_lengths));
    progress_calls = 0;
}

static size_t make_record(
    uint8_t payload_value,
    uint16_t payload_length,
    uint8_t *encoded
)
{
    obc_telemetry_record_t record = {
        .source_node = 2,
        .source_port = 20,
        .received_unix_us = UINT64_C(123456789),
        .payload_length = payload_length
    };
    memset(record.payload, payload_value, payload_length);

    size_t encoded_size = 0;
    if (
        obc_telemetry_encode(
            &record,
            encoded,
            OBC_IPC_MAX_PAYLOAD,
            &encoded_size
        ) != OBC_TELEMETRY_OK
    ) {
        return 0;
    }

    return encoded_size;
}

static void queue_record_replies(
    uint64_t record_offset,
    uint64_t following_offset,
    int end_of_log,
    const uint8_t *encoded,
    size_t encoded_size
)
{
    size_t chunk_offset = 0;

    while (chunk_offset < encoded_size && reply_count < MAX_REPLIES) {
        size_t remaining = encoded_size - chunk_offset;
        size_t chunk_length = remaining;
        if (chunk_length > OBC_TELEMETRY_READ_CHUNK_SIZE) {
            chunk_length = OBC_TELEMETRY_READ_CHUNK_SIZE;
        }

        int is_last = chunk_offset + chunk_length == encoded_size;
        obc_telemetry_read_reply_t *reply = &replies[reply_count++];
        *reply = (obc_telemetry_read_reply_t) {
            .magic = OBC_TELEMETRY_READ_REPLY_MAGIC,
            .status = OBC_TELEMETRY_READ_OK,
            .is_last_chunk = (uint8_t)is_last,
            .end_of_log = (uint8_t)(is_last && end_of_log),
            .record_offset = record_offset,
            .next_offset = following_offset,
            .record_length = (uint16_t)encoded_size,
            .chunk_offset = (uint16_t)chunk_offset,
            .chunk_length = (uint16_t)chunk_length
        };
        memcpy(reply->payload, encoded + chunk_offset, chunk_length);
        chunk_offset += chunk_length;
    }
}

int IPC_send(OBC_Roles_t destination, const uint8_t *data, uint16_t length)
{
    if (
        destination != ROLE_DATA ||
        data == NULL ||
        length != sizeof(last_request)
    ) {
        return IPC_ERROR;
    }

    memcpy(&last_request, data, sizeof(last_request));
    if (request_count < MAX_REPLIES) {
        requests[request_count] = last_request;
    }
    request_count++;
    return length;
}

int IPC_receive_timeout(
    OBC_Roles_t *source,
    uint8_t *buffer,
    uint16_t capacity,
    int timeout_ms
)
{
    (void)timeout_ms;

    if (
        source == NULL ||
        buffer == NULL ||
        capacity < sizeof(obc_telemetry_read_reply_t) ||
        next_reply >= reply_count
    ) {
        return IPC_TIMEOUT;
    }

    *source = ROLE_DATA;
    memcpy(buffer, &replies[next_reply++], sizeof(replies[0]));
    return (int)sizeof(replies[0]);
}

int radio_send(const uint8_t *data, size_t length)
{
    radio_calls++;
    if (radio_result != 0) {
        return radio_result;
    }
    if (radio_fail_on_call > 0 && radio_calls == radio_fail_on_call) {
        return -1;
    }
    if (data == NULL || length > sizeof(radio_bytes)) {
        return -1;
    }

    memcpy(radio_bytes, data, length);
    radio_length = length;
    if (radio_calls <= MAX_RADIO_CALLS) {
        memcpy(radio_history[radio_calls - 1], data, length);
        radio_history_lengths[radio_calls - 1] = length;
    }
    return 0;
}

int radio_receive(uint8_t *buffer, uint16_t max_length)
{
    (void)buffer;
    (void)max_length;
    return -1;
}

int camera_capture(const char *out_path)
{
    (void)out_path;
    return -1;
}

void mission_health_payload_progress(void)
{
    progress_calls++;
}

static void test_single_chunk_record(void)
{
    reset_fakes();

    uint8_t encoded[OBC_IPC_MAX_PAYLOAD];
    size_t encoded_size = make_record(0x11, 3, encoded);
    queue_record_replies(0, encoded_size, 0, encoded, encoded_size);

    uint64_t next_offset = UINT64_MAX;
    int end_of_log = -1;
    int result = payload_commander_downlink_telemetry_record(
        0,
        &next_offset,
        &end_of_log
    );

    check_true("single-chunk retrieval succeeds", result == 0);
    check_true(
        "Mission requested the supplied cursor",
        request_count == 1 &&
        last_request.magic == OBC_TELEMETRY_READ_REQUEST_MAGIC &&
        last_request.offset == 0
    );
    check_true(
        "single record reaches radio unchanged",
        radio_calls == 1 &&
        radio_length == encoded_size &&
        memcmp(radio_bytes, encoded, encoded_size) == 0
    );
    check_true(
        "single record advances the cursor",
        next_offset == encoded_size && !end_of_log
    );
    check_true("successful downlink reports progress", progress_calls == 1);
}

static void test_two_chunk_final_record(void)
{
    reset_fakes();

    uint8_t encoded[OBC_IPC_MAX_PAYLOAD];
    size_t encoded_size = make_record(
        0x22,
        OBC_TELEMETRY_MAX_PAYLOAD,
        encoded
    );
    const uint64_t record_offset = 100;
    const uint64_t following_offset = record_offset + encoded_size;
    queue_record_replies(
        record_offset,
        following_offset,
        1,
        encoded,
        encoded_size
    );

    uint64_t next_offset = UINT64_MAX;
    int end_of_log = 0;
    int result = payload_commander_downlink_telemetry_record(
        record_offset,
        &next_offset,
        &end_of_log
    );

    check_true("maximum record was split into two replies", reply_count == 2);
    check_true("two-chunk retrieval succeeds", result == 0);
    check_true(
        "two chunks are reassembled exactly once",
        radio_calls == 1 &&
        radio_length == encoded_size &&
        memcmp(radio_bytes, encoded, encoded_size) == 0
    );
    check_true(
        "final chunk carries end-of-log to caller",
        next_offset == following_offset && end_of_log
    );
}

static void test_radio_failure_preserves_cursor(void)
{
    reset_fakes();

    uint8_t encoded[OBC_IPC_MAX_PAYLOAD];
    size_t encoded_size = make_record(0x33, 3, encoded);
    queue_record_replies(40, 40 + encoded_size, 1, encoded, encoded_size);
    radio_result = -1;

    uint64_t next_offset = UINT64_C(777);
    int end_of_log = 7;
    int result = payload_commander_downlink_telemetry_record(
        40,
        &next_offset,
        &end_of_log
    );

    check_true("radio failure is reported", result == -1);
    check_true("radio was attempted once", radio_calls == 1);
    check_true(
        "failed radio send does not commit cursor",
        next_offset == UINT64_C(777) && end_of_log == 7
    );
    check_true("failed downlink reports no progress", progress_calls == 0);
}

static void test_end_of_log_without_record(void)
{
    reset_fakes();

    const uint64_t final_offset = 500;
    replies[0] = (obc_telemetry_read_reply_t) {
        .magic = OBC_TELEMETRY_READ_REPLY_MAGIC,
        .status = OBC_TELEMETRY_READ_END,
        .is_last_chunk = 1,
        .end_of_log = 1,
        .record_offset = final_offset,
        .next_offset = final_offset
    };
    reply_count = 1;

    uint64_t next_offset = 0;
    int end_of_log = 0;
    int result = payload_commander_downlink_telemetry_record(
        final_offset,
        &next_offset,
        &end_of_log
    );

    check_true("empty tail is a successful result", result == 0);
    check_true(
        "empty tail preserves final cursor and reports end",
        next_offset == final_offset && end_of_log
    );
    check_true("empty tail does not call radio", radio_calls == 0);
}

static void test_batch_reaches_end_of_log(void)
{
    reset_fakes();

    uint8_t encoded[3][OBC_IPC_MAX_PAYLOAD];
    size_t sizes[3];
    uint64_t offsets[4] = {0};

    for (size_t i = 0; i < 3; i++) {
        sizes[i] = make_record((uint8_t)(0x40 + i), 3, encoded[i]);
        offsets[i + 1] = offsets[i] + sizes[i];
        queue_record_replies(
            offsets[i],
            offsets[i + 1],
            i == 2,
            encoded[i],
            sizes[i]
        );
    }

    uint64_t cursor = 0;
    size_t records_sent = 99;
    int end_of_log = 0;
    int result = payload_commander_downlink_telemetry_batch(
        &cursor,
        5,
        &records_sent,
        &end_of_log
    );

    check_true("batch reaches end successfully", result == 0);
    check_true(
        "batch transmits every available record",
        records_sent == 3 && radio_calls == 3
    );
    check_true(
        "batch requests each cursor in sequence",
        request_count == 3 &&
        requests[0].offset == offsets[0] &&
        requests[1].offset == offsets[1] &&
        requests[2].offset == offsets[2]
    );
    check_true(
        "batch preserves radio transmission order",
        radio_history_lengths[0] == sizes[0] &&
        radio_history_lengths[1] == sizes[1] &&
        radio_history_lengths[2] == sizes[2] &&
        memcmp(radio_history[0], encoded[0], sizes[0]) == 0 &&
        memcmp(radio_history[1], encoded[1], sizes[1]) == 0 &&
        memcmp(radio_history[2], encoded[2], sizes[2]) == 0
    );
    check_true(
        "batch commits final cursor and end-of-log",
        cursor == offsets[3] && end_of_log
    );
}

static void test_batch_honors_record_limit(void)
{
    reset_fakes();

    uint8_t encoded[3][OBC_IPC_MAX_PAYLOAD];
    size_t sizes[3];
    uint64_t offsets[4] = {0};

    for (size_t i = 0; i < 3; i++) {
        sizes[i] = make_record((uint8_t)(0x50 + i), 3, encoded[i]);
        offsets[i + 1] = offsets[i] + sizes[i];
        queue_record_replies(
            offsets[i],
            offsets[i + 1],
            i == 2,
            encoded[i],
            sizes[i]
        );
    }

    uint64_t cursor = 0;
    size_t records_sent = 0;
    int end_of_log = 1;
    int result = payload_commander_downlink_telemetry_batch(
        &cursor,
        2,
        &records_sent,
        &end_of_log
    );

    check_true("limited batch succeeds", result == 0);
    check_true(
        "limited batch performs exactly two transmissions",
        records_sent == 2 && radio_calls == 2 && request_count == 2
    );
    check_true(
        "limited batch stops at the third record cursor",
        cursor == offsets[2] && !end_of_log
    );
}

static void test_batch_failure_keeps_failed_record_cursor(void)
{
    reset_fakes();

    uint8_t encoded[3][OBC_IPC_MAX_PAYLOAD];
    size_t sizes[3];
    uint64_t offsets[4] = {0};

    for (size_t i = 0; i < 3; i++) {
        sizes[i] = make_record((uint8_t)(0x60 + i), 3, encoded[i]);
        offsets[i + 1] = offsets[i] + sizes[i];
        queue_record_replies(
            offsets[i],
            offsets[i + 1],
            i == 2,
            encoded[i],
            sizes[i]
        );
    }
    radio_fail_on_call = 3;

    uint64_t cursor = 0;
    size_t records_sent = 0;
    int end_of_log = 0;
    int result = payload_commander_downlink_telemetry_batch(
        &cursor,
        5,
        &records_sent,
        &end_of_log
    );

    check_true("mid-batch radio failure is reported", result == -1);
    check_true(
        "records before the failure remain committed",
        records_sent == 2 && cursor == offsets[2]
    );
    check_true(
        "failed record was attempted but not committed",
        radio_calls == 3 && request_count == 3 && !end_of_log
    );
}

static void test_batch_rejects_zero_limit(void)
{
    reset_fakes();

    uint64_t cursor = 12;
    size_t records_sent = 77;
    int end_of_log = 5;
    int result = payload_commander_downlink_telemetry_batch(
        &cursor,
        0,
        &records_sent,
        &end_of_log
    );

    check_true("zero-sized batch is rejected", result == -1);
    check_true(
        "rejected batch changes no caller state",
        cursor == 12 && records_sent == 77 && end_of_log == 5
    );
    check_true("rejected batch sends no IPC or radio data", request_count == 0 && radio_calls == 0);
}

int main(void)
{
    test_single_chunk_record();
    test_two_chunk_final_record();
    test_radio_failure_preserves_cursor();
    test_end_of_log_without_record();
    test_batch_reaches_end_of_log();
    test_batch_honors_record_limit();
    test_batch_failure_keeps_failed_record_cursor();
    test_batch_rejects_zero_limit();

    if (checks_failed == 0) {
        printf("mission_telemetry_test: PASS\n");
        return 0;
    }

    fprintf(
        stderr,
        "mission_telemetry_test: FAIL (%d checks failed)\n",
        checks_failed
    );
    return 1;
}
