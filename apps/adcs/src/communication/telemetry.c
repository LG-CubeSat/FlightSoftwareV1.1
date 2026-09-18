#include "communication/telemetry.h"

#include <arpa/inet.h>
#include <math.h>
#include <stdatomic.h>
#include <string.h>

#include <csp/csp.h>

#include "control/control_math.h"
#include "csp_commands.h"

typedef struct {
    uint8_t *payload;
    size_t capacity;
    size_t used;
} encoder_t;

static atomic_uint_fast32_t next_sequence;
static atomic_uchar transport_enabled = 1U;

static uint8_t put_u8(encoder_t *encoder, uint8_t value) {
    if (encoder->used + 1U > encoder->capacity) {
        return 0U;
    }
    encoder->payload[encoder->used++] = value;
    return 1U;
}

static uint8_t put_u16(encoder_t *encoder, uint16_t value) {
    uint16_t network_value = htons(value);

    if (encoder->used + sizeof(network_value) > encoder->capacity) {
        return 0U;
    }
    memcpy(&encoder->payload[encoder->used], &network_value, sizeof(network_value));
    encoder->used += sizeof(network_value);
    return 1U;
}

static uint8_t put_u32(encoder_t *encoder, uint32_t value) {
    uint32_t network_value = htonl(value);

    if (encoder->used + sizeof(network_value) > encoder->capacity) {
        return 0U;
    }
    memcpy(&encoder->payload[encoder->used], &network_value, sizeof(network_value));
    encoder->used += sizeof(network_value);
    return 1U;
}

static uint8_t put_u64(encoder_t *encoder, uint64_t value) {
    return put_u32(encoder, (uint32_t)(value >> 32U)) &&
           put_u32(encoder, (uint32_t)value);
}

static uint8_t put_float(encoder_t *encoder, float value) {
    uint32_t representation;

    memcpy(&representation, &value, sizeof(representation));
    return put_u32(encoder, representation);
}

static uint8_t put_float_array(
    encoder_t *encoder,
    const float *values,
    size_t count) {
    for (size_t index = 0U; index < count; ++index) {
        if (!put_float(encoder, values[index])) {
            return 0U;
        }
    }
    return 1U;
}

void adcs_telemetry_init(void) {
    atomic_store(&next_sequence, 1U);
}

void adcs_telemetry_set_transport_enabled(uint8_t enabled) {
    atomic_store(&transport_enabled, enabled != 0U);
}

uint8_t adcs_telemetry_transport_is_enabled(void) {
    return atomic_load(&transport_enabled) != 0U ? 1U : 0U;
}

adcs_telemetry_status_t adcs_telemetry_encode(
    const adcs_telemetry_packet_t *telemetry,
    uint8_t *payload,
    size_t payload_capacity,
    size_t *encoded_size) {
    encoder_t encoder = {
        .payload = payload,
        .capacity = payload_capacity,
        .used = 0U
    };
    uint8_t control_flags;
    uint8_t health_flags;
    uint16_t state_flags;

    if (telemetry == NULL || payload == NULL || encoded_size == NULL ||
        telemetry->mode < ADCS_MODE_BOOT || telemetry->mode >= ADCS_MODE_COUNT ||
        !adcs_values_are_finite(telemetry->attitude.quaternion, 4U) ||
        !adcs_values_are_finite(telemetry->target.target_quaternion, 4U) ||
        !adcs_values_are_finite(
            telemetry->attitude.angular_rate_rad_s,
            ADCS_VECTOR_LENGTH) ||
        !adcs_values_are_finite(
            telemetry->sensors.magnetic_field_t,
            ADCS_VECTOR_LENGTH) ||
        !adcs_values_are_finite(
            telemetry->control.requested_dipole_a_m2,
            ADCS_VECTOR_LENGTH) ||
        !isfinite(telemetry->attitude.confidence) ||
        telemetry->attitude.confidence < 0.0F ||
        telemetry->attitude.confidence > 1.0F) {
        return ADCS_TELEMETRY_INVALID_ARGUMENT;
    }

    control_flags = (uint8_t)(
        (telemetry->control.actuators_enabled != 0U ? 1U : 0U) |
        (telemetry->control.saturated != 0U ? 2U : 0U) |
        (telemetry->control.target_settled != 0U ? 4U : 0U));
    health_flags = (uint8_t)(
        (telemetry->health.sensors_healthy != 0U ? 1U : 0U) |
        (telemetry->health.estimator_healthy != 0U ? 2U : 0U) |
        (telemetry->health.actuators_healthy != 0U ? 4U : 0U));
    state_flags = telemetry->attitude.valid != 0U
        ? ADCS_TELEMETRY_ATTITUDE_VALID_FLAG
        : 0U;

    if (!put_u8(&encoder, 'A') || !put_u8(&encoder, 'D') ||
        !put_u8(&encoder, 'C') || !put_u8(&encoder, 'S') ||
        !put_u8(&encoder, ADCS_TELEMETRY_FORMAT_VERSION) ||
        !put_u8(&encoder, (uint8_t)telemetry->mode) ||
        !put_u16(&encoder, state_flags) ||
        !put_u32(&encoder, telemetry->sequence) ||
        !put_u64(&encoder, telemetry->timestamp_us) ||
        !put_u32(&encoder, telemetry->sensors.valid_mask) ||
        !put_u32(&encoder, telemetry->health.active_faults) ||
        !put_float_array(&encoder, telemetry->attitude.quaternion, 4U) ||
        !put_float_array(&encoder, telemetry->target.target_quaternion, 4U) ||
        !put_float_array(
            &encoder,
            telemetry->attitude.angular_rate_rad_s,
            ADCS_VECTOR_LENGTH) ||
        !put_float_array(
            &encoder,
            telemetry->attitude.gyro_bias_rad_s,
            ADCS_VECTOR_LENGTH) ||
        !put_float_array(
            &encoder,
            telemetry->sensors.magnetic_field_t,
            ADCS_VECTOR_LENGTH) ||
        !put_float_array(
            &encoder,
            telemetry->sensors.sun_vector_body,
            ADCS_VECTOR_LENGTH) ||
        !put_float(&encoder, telemetry->sensors.sun_irradiance_w_m2) ||
        !put_float(&encoder, telemetry->attitude.confidence) ||
        !put_float_array(
            &encoder,
            telemetry->control.requested_torque_nm,
            ADCS_VECTOR_LENGTH) ||
        !put_float_array(
            &encoder,
            telemetry->control.requested_dipole_a_m2,
            ADCS_VECTOR_LENGTH) ||
        !put_float_array(
            &encoder,
            telemetry->control.achievable_torque_nm,
            ADCS_VECTOR_LENGTH) ||
        !put_float(&encoder, telemetry->control.pointing_error_rad) ||
        !put_u8(&encoder, control_flags) ||
        !put_u8(&encoder, health_flags) ||
        !put_u16(&encoder, 0U) ||
        !put_u32(&encoder, telemetry->health.rejected_commands) ||
        !put_u32(&encoder, telemetry->health.dropped_messages) ||
        !put_u32(&encoder, telemetry->health.estimator_resets) ||
        !put_u32(&encoder, telemetry->health.controller_errors) ||
        !put_u32(&encoder, telemetry->health.mode_transitions) ||
        !put_float(&encoder, telemetry->health.minimum_stack_margin_words)) {
        *encoded_size = 0U;
        return ADCS_TELEMETRY_BUFFER_TOO_SMALL;
    }

    *encoded_size = encoder.used;
    return ADCS_TELEMETRY_OK;
}

adcs_telemetry_status_t adcs_telemetry_send(
    const adcs_telemetry_packet_t *telemetry) {
    adcs_telemetry_packet_t snapshot;
    uint8_t payload[ADCS_TELEMETRY_MAX_PAYLOAD_SIZE];
    size_t payload_size;
    csp_conn_t *connection;
    csp_packet_t *packet;

    if (telemetry == NULL) {
        return ADCS_TELEMETRY_INVALID_ARGUMENT;
    }
    snapshot = *telemetry;
    snapshot.sequence = (uint32_t)atomic_fetch_add(&next_sequence, 1U);
    if (adcs_telemetry_encode(
            &snapshot,
            payload,
            sizeof(payload),
            &payload_size) != ADCS_TELEMETRY_OK) {
        return ADCS_TELEMETRY_INVALID_ARGUMENT;
    }
    if (atomic_load(&transport_enabled) == 0U) {
        return ADCS_TELEMETRY_OK;
    }

    connection = csp_connect(
        CSP_PRIO_NORM,
        OBC_ADDRESS,
        ADCS_TELEM_PORT,
        100,
        CSP_O_NONE);
    if (connection == NULL) {
        return ADCS_TELEMETRY_TRANSPORT_ERROR;
    }
    packet = csp_buffer_get(0);
    if (packet == NULL) {
        csp_close(connection);
        return ADCS_TELEMETRY_TRANSPORT_ERROR;
    }

    memcpy(packet->data, payload, payload_size);
    packet->length = (uint16_t)payload_size;
    csp_send(connection, packet);
    csp_close(connection);
    return ADCS_TELEMETRY_OK;
}
