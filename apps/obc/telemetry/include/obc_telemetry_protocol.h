#ifndef OBC_TELEMETRY_PROTOCOL_H
#define OBC_TELEMETRY_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#include "obc_ipc.h"

#define OBC_TELEMETRY_FORMAT_VERSION 1U
#define OBC_TELEMETRY_HEADER_SIZE 18U

#define OBC_TELEMETRY_MAX_PAYLOAD (OBC_IPC_MAX_PAYLOAD - OBC_TELEMETRY_HEADER_SIZE)

/*
So the codec is going to have these bytes
Offset: 0, Size: 4, Meaning: Magic Bytes "WESW"
Offset: 4, Size: 1, Meaning: Version
Offset: 5, Size: 1, Meaning: Source Node
Offset: 6, Size: 1, Meaning: Port Node
Offset: 7, Size: 1, Meaning: Reserved
Offset: 8, Size: 8, Meaning: Timestamp
Offset: 16, Size: 2, Meaning: Length
*/

typedef enum {
    OBC_TELEMETRY_OK = 0,
    OBC_TELEMETRY_INVALID_ARGUMENT = -1,
    OBC_TELEMETRY_BUFFER_TOO_SMALL = -2,
    OBC_TELEMETRY_INVALID_RECORD = -3,
    OBC_TELEMETRY_UNSUPPORTED_VERSION = -4
} obc_telemetry_status_t;

/* 
logical representation used by Commands and Data

This struct itself is never written to disk or sent through IPC because
the compiler may insert padding and integer byte order varies by CPU.
The encode decode functions define the actual portable wire format.
*/
typedef struct {
    uint8_t source_node;
    uint8_t source_port;
    uint64_t received_unix_us;
    uint16_t payload_length;
    uint8_t payload[OBC_TELEMETRY_MAX_PAYLOAD];
} obc_telemetry_record_t;

/*
Encodes one record into an explicitly sized, big-edian byte stream.
The encoded bytes can be sent through IPC or appended directly to disk.
*/
obc_telemetry_status_t obc_telemetry_encode(
    const obc_telemetry_record_t *record,
    uint8_t *output,
    size_t output_capacity,
    size_t *encoded_size
);

/*
Validates and decodes one complete encoded telemetry record.
*/
obc_telemetry_status_t obc_telemetry_decode(
    const uint8_t *encoded,
    size_t encoded_size,
    obc_telemetry_record_t *record
);

#endif
