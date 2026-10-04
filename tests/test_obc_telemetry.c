#include <stdio.h>
#include <string.h>

#include "obc_telemetry_protocol.h"

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

static void test_exact_encoding(void)
{
    obc_telemetry_record_t record = {
        .source_node = 2,
        .source_port = 20,
        .received_unix_us = UINT64_C(0x0102030405060708),
        .payload_length = 3,
        .payload = {0xAA, 0xBB, 0xCC}
    };

    uint8_t encoded[OBC_IPC_MAX_PAYLOAD] = {0};
    size_t encoded_size = 0;

    obc_telemetry_status_t status = obc_telemetry_encode(
        &record,
        encoded,
        sizeof(encoded),
        &encoded_size
    );

    const uint8_t expected[] = {
        /* Magic */
        'W', 'E', 'S', 'W',

        /* Version, source node, source port, reserved */
        1, 2, 20, 0,

        /* Timestamp: 0x0102030405060708 in big-endian order */
        0x01, 0x02, 0x03, 0x04,
        0x05, 0x06, 0x07, 0x08,

        /* Payload length: 3 in big-endian order */
        0x00, 0x03,

        /* Original payload */
        0xAA, 0xBB, 0xCC
    };

    check_true(
        "encoding succeeds",
        status == OBC_TELEMETRY_OK
    );

    check_true(
        "encoded size is header plus payload",
        encoded_size == sizeof(expected)
    );

    check_true(
        "encoding matches the documented wire format",
        encoded_size == sizeof(expected) &&
        memcmp(encoded, expected, sizeof(expected)) == 0
    );
}

static void test_round_trip(void)
{
    obc_telemetry_record_t original = {
        .source_node = 2,
        .source_port = 20,
        .received_unix_us = UINT64_C(1791072000123456),
        .payload_length = 4,
        .payload = {0x10, 0x20, 0x30, 0x40}
    };

    uint8_t encoded[OBC_IPC_MAX_PAYLOAD] = {0};
    size_t encoded_size = 0;

    obc_telemetry_record_t decoded;

    obc_telemetry_status_t encode_status =
        obc_telemetry_encode(
            &original,
            encoded,
            sizeof(encoded),
            &encoded_size
        );

    obc_telemetry_status_t decode_status =
        obc_telemetry_decode(
            encoded,
            encoded_size,
            &decoded
        );

    check_true(
        "round-trip encoding succeeds",
        encode_status == OBC_TELEMETRY_OK
    );

    check_true(
        "round-trip decoding succeeds",
        decode_status == OBC_TELEMETRY_OK
    );

    check_true(
        "source node survives round trip",
        decoded.source_node == original.source_node
    );

    check_true(
        "source port survives round trip",
        decoded.source_port == original.source_port
    );

    check_true(
        "receive timestamp survives round trip",
        decoded.received_unix_us == original.received_unix_us
    );

    check_true(
        "payload length survives round trip",
        decoded.payload_length == original.payload_length
    );

    check_true(
        "payload bytes survive round trip",
        memcmp(
            decoded.payload,
            original.payload,
            original.payload_length
        ) == 0
    );
}

static void test_invalid_records(void)
{
    obc_telemetry_record_t record = {
        .source_node = 2,
        .source_port = 20,
        .received_unix_us = 1234,
        .payload_length = 1,
        .payload = {0xAA}
    };

    uint8_t encoded[OBC_IPC_MAX_PAYLOAD] = {0};
    size_t encoded_size = 0;

    check_true(
        "valid setup record encodes",
        obc_telemetry_encode(
            &record,
            encoded,
            sizeof(encoded),
            &encoded_size
        ) == OBC_TELEMETRY_OK
    );

    obc_telemetry_record_t decoded;

    uint8_t saved_magic = encoded[0];
    encoded[0] = 'X';

    check_true(
        "corrupted magic is rejected",
        obc_telemetry_decode(
            encoded,
            encoded_size,
            &decoded
        ) == OBC_TELEMETRY_INVALID_RECORD
    );

    encoded[0] = saved_magic;

    uint8_t saved_version = encoded[4];
    encoded[4] = OBC_TELEMETRY_FORMAT_VERSION + 1;

    check_true(
        "unsupported version is rejected",
        obc_telemetry_decode(
            encoded,
            encoded_size,
            &decoded
        ) == OBC_TELEMETRY_UNSUPPORTED_VERSION
    );

    encoded[4] = saved_version;

    check_true(
        "truncated record is rejected",
        obc_telemetry_decode(
            encoded,
            encoded_size - 1,
            &decoded
        ) == OBC_TELEMETRY_INVALID_RECORD
    );

    encoded[7] = 1;

    check_true(
        "nonzero reserved byte is rejected",
        obc_telemetry_decode(
            encoded,
            encoded_size,
            &decoded
        ) == OBC_TELEMETRY_INVALID_RECORD
    );
}

static void test_output_buffer_limit(void)
{
    obc_telemetry_record_t record = {
        .source_node = 2,
        .source_port = 20,
        .received_unix_us = 1234,
        .payload_length = 10
    };

    uint8_t too_small[OBC_TELEMETRY_HEADER_SIZE + 9];
    size_t encoded_size = 999;

    obc_telemetry_status_t status = obc_telemetry_encode(
        &record,
        too_small,
        sizeof(too_small),
        &encoded_size
    );

    check_true(
        "undersized output buffer is rejected",
        status == OBC_TELEMETRY_BUFFER_TOO_SMALL
    );

    check_true(
        "failed encoding reports zero output bytes",
        encoded_size == 0
    );
}

int main(void)
{
    test_exact_encoding();
    test_round_trip();
    test_invalid_records();
    test_output_buffer_limit();

    if (failed_checks == 0) {
        printf(
            "obc_telemetry_test: PASS (%d/%d checks)\n",
            total_checks,
            total_checks
        );
        return 0;
    }

    fprintf(
        stderr,
        "obc_telemetry_test: FAIL (%d/%d checks failed)\n",
        failed_checks,
        total_checks
    );
    return 1;
}