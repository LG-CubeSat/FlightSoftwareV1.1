/*
 * Internal EPS messages.
 *
 * These structures carry data between the sensor, estimation, manager, and
 * telemetry layers. They are NOT CSP wire formats: telemetry encodes each
 * field explicitly so compiler padding and host byte order never leak onto
 * the bus.
 *
 * Coordinate/unit conventions used throughout EPS:
 *   - timestamps are monotonic microseconds since boot
 *   - voltages are volts, currents are amperes (positive = discharge)
 *   - temperatures are degrees Celsius
 *   - power is watts, energy is watt-hours
 *   - state of charge is a 0.0 .. 1.0 fraction
 */
#ifndef EPS_COMMUNICATION_MESSAGE_H
#define EPS_COMMUNICATION_MESSAGE_H

#include <stdint.h>

#include "power_rail.h"

typedef enum {
    EPS_RESULT_OK = 0,
    EPS_RESULT_INVALID_ARGUMENT = -1,
    EPS_RESULT_NOT_INITIALIZED = -2,
    EPS_RESULT_INVALID_DATA = -3,
    EPS_RESULT_OUT_OF_RANGE = -4,
    EPS_RESULT_UNAVAILABLE = -5
} eps_result_t;

typedef enum {
    EPS_MODE_BOOT = 0,
    EPS_MODE_SAFE = 1,
    EPS_MODE_NOMINAL = 2,
    EPS_MODE_LOW_POWER = 3,
    EPS_MODE_COUNT
} eps_mode_t;

typedef enum {
    EPS_SENSOR_VALID_NONE = 0U,
    EPS_SENSOR_VALID_BATTERY = 1U << 0,
    EPS_SENSOR_VALID_SOLAR = 1U << 1,
    EPS_SENSOR_VALID_RAILS = 1U << 2
} eps_sensor_validity_t;

typedef enum {
    EPS_COMMAND_NONE = 0,
    EPS_COMMAND_SET_MODE,
    EPS_COMMAND_SET_RAIL,
    EPS_COMMAND_POWER_CYCLE,
    EPS_COMMAND_SET_UNIX_TIME
} eps_command_type_t;

typedef struct {
    uint32_t sequence;
    uint64_t timestamp_us;
    float pack_voltage_v;
    float pack_current_a;
    float battery_temperature_c;
    float state_of_charge;
    float solar_voltage_v;
    float solar_current_a;
    float solar_irradiance_w_m2;
    float rail_voltage_v[POWER_RAIL_COUNT];
    float rail_current_a[POWER_RAIL_COUNT];
    uint8_t rail_enabled[POWER_RAIL_COUNT];
    uint32_t valid_mask;
} eps_sensor_packet_t;

typedef struct {
    uint64_t timestamp_us;
    float instant_power_w;
    float average_power_w;
    float energy_wh;
    float battery_efficiency;
    uint8_t power_positive;   /* nonzero when net charging */
} eps_power_state_t;

typedef struct {
    uint64_t timestamp_us;
    uint32_t active_faults;
    uint32_t rejected_commands;
    uint32_t dropped_messages;
    uint32_t mode_transitions;
    uint32_t rail_switches;
    float minimum_stack_margin_words;
    uint8_t sensors_healthy;
} eps_health_t;

typedef struct {
    uint32_t sequence;
    eps_command_type_t type;
    union {
        eps_mode_t mode;
        struct {
            uint8_t rail_id;
            uint8_t enabled;
        } rail;
        uint8_t power_cycle_rail_id;
        int64_t unix_time_sec;
    } parameter;
} eps_command_t;

typedef struct {
    uint32_t sequence;
    uint64_t timestamp_us;
    eps_mode_t mode;
    eps_sensor_packet_t sensors;
    eps_power_state_t power;
    eps_health_t health;
} eps_telemetry_packet_t;

#endif
