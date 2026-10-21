#ifndef OBC_TELEMETRY_PROTOCOL_H
#define OBC_TELEMETRY_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#include "obc_ipc.h"

#define OBC_TELEMETRY_FORMAT_VERSION 1U
#define OBC_TELEMETRY_HEADER_SIZE 18U

#define OBC_TELEMETRY_MAX_PAYLOAD (OBC_IPC_MAX_PAYLOAD - OBC_TELEMETRY_HEADER_SIZE)

/*
These messages are internal OBC IPC structures. Unlike encoded telemetry records
these are never written to disk or transmitted to ground.
*/
#define OBC_TELEMETRY_READ_REQUEST_MAGIC UINT32_C(0x544C4D51) /* "TLMQ" */
#define OBC_TELEMETRY_READ_REPLY_MAGIC UINT32_C(0x544C4D41) /* "TLMA" */

#define OBC_TELEMETRY_READ_CHUNK_SIZE 216U

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

typedef enum {
    OBC_TELEMETRY_READ_OK = 0,
    OBC_TELEMETRY_READ_END = 1,
    OBC_TELEMETRY_READ_IO_ERROR = -1,
    OBC_TELEMETRY_READ_INVALID_RECORD = -2,
    OBC_TELEMETRY_READ_INVALID_ARGUMENT = -3
} obc_telemetry_read_status_t;

/*
Mission -> Data
offset is a byte position in the append-only telemetry log.
*/
typedef struct {
    uint32_t magic;
    uint64_t offset;
} obc_telemetry_read_request_t;

/*
Data -> Mission
One encoded record may require multiple replies. 
All replies for that record share record_offset, next_offset, and record_length.
*/
typedef struct {
    uint32_t magic;
    int8_t status;

    uint8_t is_last_chunk;
    uint8_t end_of_log;
    uint8_t reserved;

    uint64_t record_offset;
    uint64_t next_offset;

    uint16_t record_length;
    uint16_t chunk_offset;
    uint16_t chunk_length;

    uint8_t payload[OBC_TELEMETRY_READ_CHUNK_SIZE];
} obc_telemetry_read_reply_t;

_Static_assert(
    sizeof(obc_telemetry_read_reply_t) <= OBC_IPC_MAX_PAYLOAD,
    "telemetry read reply must fit inside one IPC message"
);

/* 
logical representation used by Commands and Data

This struct itself is never written to disk or sent through IPC because
the compiler may insert padding and integer byte order varies by CPU.
The encode decode functions define the actual portable wire format.
*/
typedef struct {
    uint8_t source_node;
    uint8_t source_port;
    uint64_t received_unix_us; // unix is since 1970 midnight UTC. us is for microseconds.
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
