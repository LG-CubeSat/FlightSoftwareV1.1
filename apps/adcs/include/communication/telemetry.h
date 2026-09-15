/* Packages ADCS state into an explicitly encoded CSP telemetry payload. */
#ifndef ADCS_COMMUNICATION_TELEMETRY_H
#define ADCS_COMMUNICATION_TELEMETRY_H

#include <stddef.h>
#include <stdint.h>

#include "communication/message.h"

#define ADCS_TELEMETRY_MAX_PAYLOAD_SIZE 256U
#define ADCS_TELEMETRY_FORMAT_VERSION 1U

/*
 * Version 1 is big-endian and begins with "ADCS", version, mode, reserved,
 * sequence, and timestamp. It then contains validity/fault masks; current and
 * target quaternions; estimated rate and bias; magnetic, Sun, temperature,
 * and irradiance measurements; control vectors/error/flags; and health
 * counters. Use adcs_telemetry_encode rather than copying native structures.
 */

typedef enum {
    ADCS_TELEMETRY_OK = 0,
    ADCS_TELEMETRY_INVALID_ARGUMENT = -1,
    ADCS_TELEMETRY_BUFFER_TOO_SMALL = -2,
    ADCS_TELEMETRY_TRANSPORT_ERROR = -3
} adcs_telemetry_status_t;

/* Initializes telemetry sequence counters and any transport-local state. */
void adcs_telemetry_init(void);

/* Enables CSP transport, or keeps encoding/logging local for standalone SIM. */
void adcs_telemetry_set_transport_enabled(uint8_t enabled);

/*
 * Serializes one telemetry snapshot without copying native struct padding or
 * native-endian values onto the CSP link.
 */
adcs_telemetry_status_t adcs_telemetry_encode(
    const adcs_telemetry_packet_t *telemetry,
    uint8_t *payload,
    size_t payload_capacity,
    size_t *encoded_size);

/* Encodes and sends one snapshot to the OBC on ADCS_TELEM_PORT. */
adcs_telemetry_status_t adcs_telemetry_send(
    const adcs_telemetry_packet_t *telemetry);

#endif
