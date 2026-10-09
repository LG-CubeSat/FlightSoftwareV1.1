#include "communication/telemetry.h"

#include <arpa/inet.h>
#include <math.h>
#include <stdatomic.h>
#include <string.h>

#include <csp/csp.h>

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

static uint8_t put_u8_array(
    encoder_t *encoder,
    const uint8_t *values,
    size_t count) {
    for (size_t index = 0U; index < count; ++index) {
        if (!put_u8(encoder, values[index])) {
            return 0U;
        }
    }
    return 1U;
}

void eps_telemetry_init(void) {
    atomic_store(&next_sequence, 1U);
}

void eps_telemetry_set_transport_enabled(uint8_t enabled) {
    atomic_store(&transport_enabled, enabled != 0U);
}

uint8_t eps_telemetry_transport_is_enabled(void) {
    return atomic_load(&transport_enabled) != 0U ? 1U : 0U;
}

eps_telemetry_status_t eps_telemetry_encode(
    const eps_telemetry_packet_t *telemetry,
    uint8_t *payload,
    size_t payload_capacity,
    size_t *encoded_size) {
    encoder_t encoder = {
        .payload = payload,
        .capacity = payload_capacity,
        .used = 0U
    };
    uint16_t state_flags;

    if (telemetry == NULL || payload == NULL || encoded_size == NULL ||
        telemetry->mode < EPS_MODE_BOOT || telemetry->mode >= EPS_MODE_COUNT ||
        !isfinite(telemetry->sensors.pack_voltage_v) ||
        !isfinite(telemetry->sensors.pack_current_a) ||
        !isfinite(telemetry->sensors.battery_temperature_c) ||
        !isfinite(telemetry->sensors.state_of_charge) ||
        !isfinite(telemetry->power.instant_power_w)) {
        return EPS_TELEMETRY_INVALID_ARGUMENT;
    }

    state_flags = telemetry->health.sensors_healthy != 0U ? 1U : 0U;

    if (!put_u8(&encoder, 'E') || !put_u8(&encoder, 'P') ||
        !put_u8(&encoder, 'S') || !put_u8(&encoder, EPS_TELEMETRY_FORMAT_VERSION) ||
        !put_u8(&encoder, (uint8_t)telemetry->mode) ||
        !put_u16(&encoder, state_flags) ||
        !put_u32(&encoder, telemetry->sequence) ||
        !put_u64(&encoder, telemetry->timestamp_us) ||
        !put_u32(&encoder, telemetry->sensors.valid_mask) ||
        !put_float(&encoder, telemetry->sensors.pack_voltage_v) ||
        !put_float(&encoder, telemetry->sensors.pack_current_a) ||
        !put_float(&encoder, telemetry->sensors.battery_temperature_c) ||
        !put_float(&encoder, telemetry->sensors.state_of_charge) ||
        !put_float(&encoder, telemetry->sensors.solar_voltage_v) ||
        !put_float(&encoder, telemetry->sensors.solar_current_a) ||
        !put_float(&encoder, telemetry->sensors.solar_irradiance_w_m2) ||
        !put_float_array(
            &encoder,
            telemetry->sensors.rail_voltage_v,
            POWER_RAIL_COUNT) ||
        !put_float_array(
            &encoder,
            telemetry->sensors.rail_current_a,
            POWER_RAIL_COUNT) ||
        !put_u8_array(
            &encoder,
            telemetry->sensors.rail_enabled,
            POWER_RAIL_COUNT) ||
        !put_float(&encoder, telemetry->power.instant_power_w) ||
        !put_float(&encoder, telemetry->power.average_power_w) ||
        !put_float(&encoder, telemetry->power.energy_wh) ||
        !put_float(&encoder, telemetry->power.battery_efficiency) ||
        !put_u8(&encoder, telemetry->power.power_positive != 0U ? 1U : 0U) ||
        !put_u32(&encoder, telemetry->health.active_faults) ||
        !put_u32(&encoder, telemetry->health.rejected_commands) ||
        !put_u32(&encoder, telemetry->health.dropped_messages) ||
        !put_u32(&encoder, telemetry->health.mode_transitions) ||
        !put_u32(&encoder, telemetry->health.rail_switches) ||
        !put_float(&encoder, telemetry->health.minimum_stack_margin_words) ||
        !put_u8(&encoder, telemetry->health.sensors_healthy != 0U ? 1U : 0U)) {
        *encoded_size = 0U;
        return EPS_TELEMETRY_BUFFER_TOO_SMALL;
    }

    *encoded_size = encoder.used;
    return EPS_TELEMETRY_OK;
}

eps_telemetry_status_t eps_telemetry_send(
    const eps_telemetry_packet_t *telemetry) {
    eps_telemetry_packet_t snapshot;
    uint8_t payload[EPS_TELEMETRY_MAX_PAYLOAD_SIZE];
    size_t payload_size;
    csp_conn_t *connection;
    csp_packet_t *packet;

    if (telemetry == NULL) {
        return EPS_TELEMETRY_INVALID_ARGUMENT;
    }
    snapshot = *telemetry;
    snapshot.sequence = (uint32_t)atomic_fetch_add(&next_sequence, 1U);
    if (eps_telemetry_encode(
            &snapshot,
            payload,
            sizeof(payload),
            &payload_size) != EPS_TELEMETRY_OK) {
        return EPS_TELEMETRY_INVALID_ARGUMENT;
    }
    if (atomic_load(&transport_enabled) == 0U) {
        return EPS_TELEMETRY_OK;
    }

    connection = csp_connect(
        CSP_PRIO_NORM,
        OBC_ADDRESS,
        EPS_TELEM_PORT,
        100,
        CSP_O_NONE);
    if (connection == NULL) {
        return EPS_TELEMETRY_TRANSPORT_ERROR;
    }
    packet = csp_buffer_get(0);
    if (packet == NULL) {
        csp_close(connection);
        return EPS_TELEMETRY_TRANSPORT_ERROR;
    }

    memcpy(packet->data, payload, payload_size);
    packet->length = (uint16_t)payload_size;
    csp_send(connection, packet);
    csp_close(connection);
    return EPS_TELEMETRY_OK;
}
