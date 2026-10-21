#include "obc_telemetry_protocol.h"

#include <string.h>

#define MAGIC_OFFSET 0U
#define VERSION_OFFSET 4U
#define SOURCE_NODE_OFFSET 5U
#define SOURCE_PORT_OFFSET 6U
#define RESERVED_OFFSET 7U
#define TIMESTAMP_OFFSET 8U
#define LENGTH_OFFSET 16U

static const uint8_t telemetry_magic[4] = {
    'W', 'E', 'S', 'W'
};

static void put_u16_be(uint8_t *output, uint16_t value)
{
    output[0] = (uint8_t)(value >> 8U);
    output[1] = (uint8_t)value;
}

static uint16_t get_u16_be(const uint8_t *input)
{
    return ((uint16_t)input[0] << 8U) | (uint16_t)input[1];
}

// puts the most signficant byte 
static void put_u64_be(uint8_t *output, uint64_t value)
{
    for (size_t i=0; i < sizeof(value); i++) {
        unsigned shift = (unsigned)(56U - (i * 8U));
        output[i] = (uint8_t)(value >> shift);
    }
}

// puts into reverse: least sig byte
static uint64_t get_u64_be(const uint8_t *input)
{
    uint64_t value = 0;
    for (size_t i=0; i<sizeof(value); i++) {
        value = (value << 8U) | input[i];
    }

    return value;
}

obc_telemetry_status_t obc_telemetry_encode(
    const obc_telemetry_record_t *record,
    uint8_t *output,
    size_t output_capacity,
    size_t *encoded_size
)
{
    // check null pointers
    if (
        record == NULL ||
        output == NULL ||
        encoded_size == NULL
    ) {
        return OBC_TELEMETRY_INVALID_ARGUMENT;
    }

    // failed encodings should never let the caller believe bytes were encoded if it messes up.
    *encoded_size = 0;

    if (record->payload_length > OBC_TELEMETRY_MAX_PAYLOAD) {
        return OBC_TELEMETRY_INVALID_ARGUMENT;
    }

    size_t required_size = OBC_TELEMETRY_HEADER_SIZE + record->payload_length;

    if (output_capacity < required_size) {
        return OBC_TELEMETRY_BUFFER_TOO_SMALL;
    }

    // header
    memcpy(
        &output[MAGIC_OFFSET],
        telemetry_magic,
        sizeof(telemetry_magic)
    );

    output[VERSION_OFFSET] = OBC_TELEMETRY_FORMAT_VERSION;
    output[SOURCE_NODE_OFFSET] = record->source_node;
    output[SOURCE_PORT_OFFSET] = record->source_port;
    output[RESERVED_OFFSET] = 0;

    /*
    Multi-byte integers must use the defined big endian wire order
    */
    put_u64_be(
        &output[TIMESTAMP_OFFSET],
        record->received_unix_us
    );
    
    put_u16_be(
        &output[LENGTH_OFFSET],
        record->payload_length
    );

    /*
    Preserve the original board telemetry bytes exactly.
    */
    memcpy(
        &output[OBC_TELEMETRY_HEADER_SIZE],
        record->payload,
        record->payload_length
    );

    *encoded_size = required_size;
    return OBC_TELEMETRY_OK;
}

obc_telemetry_status_t obc_telemetry_decode(
    const uint8_t *encoded,
    size_t encoded_size,
    obc_telemetry_record_t *record
)
{
    if (encoded == NULL || record == NULL) {
        return OBC_TELEMETRY_INVALID_ARGUMENT;
    }

    if (encoded_size < OBC_TELEMETRY_HEADER_SIZE) {
        return OBC_TELEMETRY_INVALID_RECORD;
    }

    if (
        memcmp(
            &encoded[MAGIC_OFFSET],
            telemetry_magic,
            sizeof(telemetry_magic)
        ) != 0
    ) {
        return OBC_TELEMETRY_INVALID_RECORD;
    }

    if (encoded[VERSION_OFFSET] != OBC_TELEMETRY_FORMAT_VERSION) {
        return OBC_TELEMETRY_UNSUPPORTED_VERSION;
    }

    if (encoded[RESERVED_OFFSET] != 0) {
        return OBC_TELEMETRY_INVALID_RECORD;
    }

    uint16_t payload_length = get_u16_be(&encoded[LENGTH_OFFSET]);

    if (payload_length > OBC_TELEMETRY_MAX_PAYLOAD) {
        return OBC_TELEMETRY_INVALID_RECORD;
    }

    size_t expected_size = OBC_TELEMETRY_HEADER_SIZE + payload_length;

    if (encoded_size != expected_size) {
        return OBC_TELEMETRY_INVALID_RECORD;
    }

    memset(record, 0, sizeof(*record));

    record->source_node = encoded[SOURCE_NODE_OFFSET];
    record->source_port = encoded[SOURCE_PORT_OFFSET];
    record->received_unix_us = get_u64_be(&encoded[TIMESTAMP_OFFSET]);
    record->payload_length = payload_length;

    memcpy(
        record->payload,
        &encoded[OBC_TELEMETRY_HEADER_SIZE],
        payload_length
    );

    return OBC_TELEMETRY_OK;
}
