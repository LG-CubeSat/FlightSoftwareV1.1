#ifndef OBC_DATA_TELEMETRY_STORE_H
#define OBC_DATA_TELEMETRY_STORE_H

#include <stddef.h>
#include <stdint.h>

// Validate and append one complete encoded telemetry record.
int telemetry_store_append(
    const uint8_t *encoded,
    size_t encoded_size
);

#endif