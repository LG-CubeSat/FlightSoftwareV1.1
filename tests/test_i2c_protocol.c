#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "i2c_protocol.h"

static int failures = 0;

#define CHECK(condition, message) do {                   \
    if (condition) {                                     \
        printf("[PASS] %s\n", message);                  \
    } else {                                             \
        fprintf(stderr, "[FAIL] %s\n", message);         \
        failures++;                                      \
    }                                                    \
} while (0)

static void test_idle_status(void)
{
    I2cStatus_t status = {
        .flags = 0U,
        .frame_length = 0U,
    };

    uint8_t wire[I2C_STATUS_WIRE_SIZE] = {0};

    CHECK(
        i2c_status_encode(&status, wire, sizeof(wire)) == 0,
        "idle status encodes successfully"
    );

    CHECK(
        wire[0] == I2C_PROTOCOL_MAGIC &&
        wire[1] == I2C_PROTOCOL_VERSION &&
        wire[2] == 0U &&
        wire[3] == 0U &&
        wire[4] == 0U,
        "idle status has the expected wire representation"
    );

    I2cStatus_t decoded = {
        .flags = 0xFFU,
        .frame_length = UINT16_MAX,
    };

    CHECK(
        i2c_status_decode(wire, sizeof(wire), &decoded) == 0,
        "idle status decodes successfully"
    );

    CHECK(
        decoded.flags == 0U &&
        decoded.frame_length == 0U,
        "decoded idle status reports no pending data"
    );
}

static void test_ready_status(void)
{
    I2cStatus_t status = {
        .flags = I2C_STATUS_DATA_READY,
        .frame_length = MAX_FRAME_WIRE_SIZE,
    };

    uint8_t wire[I2C_STATUS_WIRE_SIZE] = {0};

    CHECK(
        i2c_status_encode(&status, wire, sizeof(wire)) == 0,
        "data-ready status encodes successfully"
    );

    CHECK(
        wire[3] == (uint8_t)(MAX_FRAME_WIRE_SIZE >> 8) &&
        wire[4] == (uint8_t)(MAX_FRAME_WIRE_SIZE & 0xFFU),
        "frame length is encoded in big-endian order"
    );

    I2cStatus_t decoded = {0};

    CHECK(
        i2c_status_decode(wire, sizeof(wire), &decoded) == 0,
        "data-ready status decodes successfully"
    );

    CHECK(
        decoded.flags == I2C_STATUS_DATA_READY &&
        decoded.frame_length == MAX_FRAME_WIRE_SIZE,
        "decoded status preserves ready flag and frame length"
    );
}

static void test_invalid_field_combinations(void)
{
    uint8_t wire[I2C_STATUS_WIRE_SIZE] = {0};

    I2cStatus_t idle_with_length = {
        .flags = 0U,
        .frame_length = FRAME_HEADER_SIZE,
    };

    CHECK(
        i2c_status_encode(
            &idle_with_length,
            wire,
            sizeof(wire)
        ) == -1,
        "idle status rejects a nonzero frame length"
    );

    I2cStatus_t ready_too_short = {
        .flags = I2C_STATUS_DATA_READY,
        .frame_length = FRAME_HEADER_SIZE - 1U,
    };

    CHECK(
        i2c_status_encode(
            &ready_too_short,
            wire,
            sizeof(wire)
        ) == -1,
        "ready status rejects a frame shorter than its header"
    );

    I2cStatus_t ready_too_large = {
        .flags = I2C_STATUS_DATA_READY,
        .frame_length = MAX_FRAME_WIRE_SIZE + 1U,
    };

    CHECK(
        i2c_status_encode(
            &ready_too_large,
            wire,
            sizeof(wire)
        ) == -1,
        "ready status rejects an oversized frame"
    );

    I2cStatus_t unknown_flags = {
        .flags = 0x80U,
        .frame_length = 0U,
    };

    CHECK(
        i2c_status_encode(
            &unknown_flags,
            wire,
            sizeof(wire)
        ) == -1,
        "status rejects unknown flag bits"
    );
}

static void test_malformed_wire_responses(void)
{
    I2cStatus_t output = {
        .flags = 0x55U,
        .frame_length = 123U,
    };

    const I2cStatus_t original_output = output;

    uint8_t all_ff[I2C_STATUS_WIRE_SIZE];
    memset(all_ff, 0xFF, sizeof(all_ff));

    CHECK(
        i2c_status_decode(
            all_ff,
            sizeof(all_ff),
            &output
        ) == -1,
        "decoder rejects an uninitialized 0xFF response"
    );

    CHECK(
        output.flags == original_output.flags &&
        output.frame_length == original_output.frame_length,
        "failed decoding does not modify caller output"
    );

    uint8_t bad_magic[I2C_STATUS_WIRE_SIZE] = {
        0x00U,
        I2C_PROTOCOL_VERSION,
        0U,
        0U,
        0U,
    };

    CHECK(
        i2c_status_decode(
            bad_magic,
            sizeof(bad_magic),
            &output
        ) == -1,
        "decoder rejects an invalid magic byte"
    );

    uint8_t bad_version[I2C_STATUS_WIRE_SIZE] = {
        I2C_PROTOCOL_MAGIC,
        I2C_PROTOCOL_VERSION + 1U,
        0U,
        0U,
        0U,
    };

    CHECK(
        i2c_status_decode(
            bad_version,
            sizeof(bad_version),
            &output
        ) == -1,
        "decoder rejects an unsupported protocol version"
    );

    uint8_t truncated[I2C_STATUS_WIRE_SIZE - 1U] = {0};

    CHECK(
        i2c_status_decode(
            truncated,
            sizeof(truncated),
            &output
        ) == -1,
        "decoder rejects a truncated response"
    );
}

static void test_invalid_arguments(void)
{
    I2cStatus_t status = {
        .flags = 0U,
        .frame_length = 0U,
    };

    uint8_t wire[I2C_STATUS_WIRE_SIZE] = {0};

    CHECK(
        i2c_status_encode(NULL, wire, sizeof(wire)) == -1,
        "encoder rejects a null status"
    );

    CHECK(
        i2c_status_encode(&status, NULL, sizeof(wire)) == -1,
        "encoder rejects a null output buffer"
    );

    CHECK(
        i2c_status_encode(
            &status,
            wire,
            I2C_STATUS_WIRE_SIZE - 1U
        ) == -1,
        "encoder rejects an undersized output buffer"
    );

    CHECK(
        i2c_status_decode(NULL, sizeof(wire), &status) == -1,
        "decoder rejects a null input buffer"
    );

    CHECK(
        i2c_status_decode(wire, sizeof(wire), NULL) == -1,
        "decoder rejects a null output structure"
    );
}

int main(void)
{
    test_idle_status();
    test_ready_status();
    test_invalid_field_combinations();
    test_malformed_wire_responses();
    test_invalid_arguments();

    if (failures != 0) {
        fprintf(
            stderr,
            "i2c_protocol_test: FAIL (%d checks failed)\n",
            failures
        );
        return 1;
    }

    printf("i2c_protocol_test: PASS\n");
    return 0;
}