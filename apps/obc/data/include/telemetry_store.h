#ifndef OBC_DATA_TELEMETRY_STORE_H
#define OBC_DATA_TELEMETRY_STORE_H

#include <stddef.h>
#include <stdint.h>

#include "obc_telemetry_protocol.h"

// Validate and append one complete encoded telemetry record.
int telemetry_store_append(
    const uint8_t *encoded,
    size_t encoded_size
);

/* Read and validate one complete record beginning at offset. */
obc_telemetry_read_status_t telemetry_store_read_record(
    uint64_t offset,
    uint8_t *encoded_out,
    size_t encoded_capacity,
    size_t *record_size_out,
    uint64_t *next_offset_out,
    int *end_of_log_out
);

#endif
