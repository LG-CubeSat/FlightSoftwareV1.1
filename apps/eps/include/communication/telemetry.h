/* Packages EPS state into an explicitly encoded CSP telemetry payload. */
#ifndef EPS_COMMUNICATION_TELEMETRY_H
#define EPS_COMMUNICATION_TELEMETRY_H

#include <stddef.h>
#include <stdint.h>

#include "communication/message.h"

#define EPS_TELEMETRY_MAX_PAYLOAD_SIZE 256U
#define EPS_TELEMETRY_FORMAT_VERSION 1U

/*
 * Version 1 is big-endian and begins with "EPS", version, mode, state flags,
 * sequence, and timestamp. It then carries the battery/solar/rail sensor
 * block, the derived power state, and health counters. Use
 * eps_telemetry_encode rather than copying native structures.
 */

typedef enum {
    EPS_TELEMETRY_OK = 0,
    EPS_TELEMETRY_INVALID_ARGUMENT = -1,
    EPS_TELEMETRY_BUFFER_TOO_SMALL = -2,
    EPS_TELEMETRY_TRANSPORT_ERROR = -3
} eps_telemetry_status_t;

/* Initializes telemetry sequence counters and any transport-local state. */
void eps_telemetry_init(void);

/* Enables CSP transport, or keeps encoding/logging local for standalone SIM. */
void eps_telemetry_set_transport_enabled(uint8_t enabled);

/* Returns whether CSP telemetry transport is enabled for this process. */
uint8_t eps_telemetry_transport_is_enabled(void);

/*
 * Serializes one telemetry snapshot without copying native struct padding or
 * native-endian values onto the CSP link.
 */
eps_telemetry_status_t eps_telemetry_encode(
    const eps_telemetry_packet_t *telemetry,
    uint8_t *payload,
    size_t payload_capacity,
    size_t *encoded_size);

/* Encodes and sends one snapshot to the OBC on EPS_TELEM_PORT. */
eps_telemetry_status_t eps_telemetry_send(
    const eps_telemetry_packet_t *telemetry);

#endif
