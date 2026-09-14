/*
 * Internal ADCS messages.
 *
 * These structures carry data between the sensor, estimation, control,
 * manager, and telemetry layers. They are not CSP wire formats: telemetry
 * must encode each field explicitly so compiler padding and host byte order
 * never leak onto the bus.
 *
 * Coordinate conventions used throughout ADCS:
 *   - quaternions use cglm versor layout [x, y, z, w] and rotate ECI into body
 *   - sensor/estimate/control timestamps are monotonic microseconds since boot
 *   - angular rates are body-frame radians/second
 *   - magnetic fields are body- or ECI-frame tesla as named
 *   - control torques are body-frame newton-metres
 *   - reaction-wheel commands state torque applied to the spacecraft body
 *   - magnetorquer dipoles are body-frame ampere-square-metres
 *   - inertial vectors use Earth-centred inertial (ECI) coordinates
 */
#ifndef ADCS_COMMUNICATION_MESSAGE_H
#define ADCS_COMMUNICATION_MESSAGE_H

#include <stdint.h>
#include <cglm/types.h>

#define ADCS_VECTOR_LENGTH             3U
#define ADCS_ERROR_STATE_LENGTH        6U
#define ADCS_COVARIANCE_ELEMENT_COUNT 36U

typedef enum {
    ADCS_RESULT_OK = 0,
    ADCS_RESULT_INVALID_ARGUMENT = -1,
    ADCS_RESULT_NOT_INITIALIZED = -2,
    ADCS_RESULT_INVALID_DATA = -3,
    ADCS_RESULT_OUT_OF_RANGE = -4,
    ADCS_RESULT_UNAVAILABLE = -5
} adcs_result_t;

typedef enum {
    ADCS_MODE_BOOT = 0,
    ADCS_MODE_SAFE = 1,
    ADCS_MODE_DETUMBLE = 2,
    ADCS_MODE_SUN_ACQUISITION = 3,
    ADCS_MODE_SUN_POINTING = 4,
    ADCS_MODE_EARTH_POINTING = 5,
    ADCS_MODE_SLEWING = 6,
    ADCS_MODE_TARGET_POINTING = 7,
    ADCS_MODE_SCIENCE = 8,
    ADCS_MODE_COUNT
} adcs_mode_t;

typedef enum {
    ADCS_SENSOR_VALID_NONE = 0U,
    ADCS_SENSOR_VALID_GYROSCOPE = 1U << 0,
    ADCS_SENSOR_VALID_ACCELEROMETER = 1U << 1,
    ADCS_SENSOR_VALID_MAGNETOMETER = 1U << 2,
    ADCS_SENSOR_VALID_SUN = 1U << 3,
    ADCS_SENSOR_VALID_TEMPERATURE = 1U << 4
} adcs_sensor_validity_t;

typedef enum {
    ADCS_REFERENCE_VALID_NONE = 0U,
    ADCS_REFERENCE_VALID_SUN = 1U << 0,
    ADCS_REFERENCE_VALID_MAGNETIC_FIELD = 1U << 1,
    ADCS_REFERENCE_VALID_NADIR = 1U << 2
} adcs_reference_validity_t;

typedef enum {
    ADCS_COMMAND_NONE = 0,
    ADCS_COMMAND_SET_MODE,
    ADCS_COMMAND_SET_ATTITUDE,
    ADCS_COMMAND_SET_POINTING_VECTOR,
    ADCS_COMMAND_RESET_ESTIMATOR,
    ADCS_COMMAND_DISABLE_ACTUATORS,
    ADCS_COMMAND_SET_UNIX_TIME,
    ADCS_COMMAND_SIMULATOR_FAULT,
    ADCS_COMMAND_LEGACY_POSITION
} adcs_command_type_t;

typedef struct {
    uint32_t sequence;
    uint64_t timestamp_us;
    float angular_rate_rad_s[ADCS_VECTOR_LENGTH];
    float acceleration_m_s2[ADCS_VECTOR_LENGTH];
    float magnetic_field_t[ADCS_VECTOR_LENGTH];
    float sun_vector_body[ADCS_VECTOR_LENGTH];
    float sun_irradiance_w_m2;
    float board_temperature_c;
    uint32_t valid_mask;
} adcs_sensor_packet_t;

typedef struct {
    uint64_t unix_time_us;
    float position_eci_m[ADCS_VECTOR_LENGTH];
    float velocity_eci_m_s[ADCS_VECTOR_LENGTH];
    uint8_t valid;
} adcs_orbit_state_t;

typedef struct {
    uint64_t unix_time_us;
    float sun_eci_unit[ADCS_VECTOR_LENGTH];
    float magnetic_field_eci_t[ADCS_VECTOR_LENGTH];
    float nadir_eci_unit[ADCS_VECTOR_LENGTH];
    uint32_t valid_mask;
} adcs_reference_vectors_t;

typedef struct {
    uint64_t timestamp_us;
    versor quaternion;
    float angular_rate_rad_s[ADCS_VECTOR_LENGTH];
    float gyro_bias_rad_s[ADCS_VECTOR_LENGTH];
    float covariance_diagonal[ADCS_ERROR_STATE_LENGTH];
    float confidence;
    uint8_t valid;
} adcs_attitude_state_t;

typedef struct {
    adcs_mode_t mode;
    versor target_quaternion;
    float pointing_axis_body[ADCS_VECTOR_LENGTH];
    float target_direction_eci[ADCS_VECTOR_LENGTH];
    float maximum_rate_rad_s;
} adcs_guidance_target_t;

typedef struct {
    uint64_t timestamp_us;
    float requested_torque_nm[ADCS_VECTOR_LENGTH];
    float reaction_wheel_torque_nm[ADCS_VECTOR_LENGTH];
    float requested_dipole_a_m2[ADCS_VECTOR_LENGTH];
    float achievable_torque_nm[ADCS_VECTOR_LENGTH];
    float pointing_error_rad;
    uint8_t actuators_enabled;
    uint8_t reaction_wheels_enabled;
    uint8_t saturated;
    uint8_t target_settled;
} adcs_control_output_t;

typedef struct {
    uint64_t timestamp_us;
    float dipole_a_m2[ADCS_VECTOR_LENGTH];
    uint8_t enabled;
} adcs_magnetorquer_command_t;

typedef struct {
    uint64_t timestamp_us;
    float body_torque_nm[ADCS_VECTOR_LENGTH];
    uint8_t enabled;
} adcs_reaction_wheel_command_t;

typedef struct {
    uint64_t timestamp_us;
    uint32_t active_faults;
    uint32_t rejected_commands;
    uint32_t dropped_messages;
    uint32_t estimator_resets;
    uint32_t controller_errors;
    uint32_t mode_transitions;
    float minimum_stack_margin_words;
    uint8_t sensors_healthy;
    uint8_t estimator_healthy;
    uint8_t actuators_healthy;
} adcs_health_t;

typedef struct {
    uint32_t sequence;
    adcs_command_type_t type;
    union {
        adcs_mode_t mode;
        versor attitude;
        struct {
            float body_axis[ADCS_VECTOR_LENGTH];
            float inertial_direction[ADCS_VECTOR_LENGTH];
        } pointing;
        int64_t unix_time_sec;
        uint8_t actuators_inhibited;
        uint32_t simulator_fault_mask;
        int32_t legacy_position;
    } parameter;
} adcs_command_t;

typedef struct {
    uint32_t sequence;
    uint64_t timestamp_us;
    adcs_mode_t mode;
    adcs_sensor_packet_t sensors;
    adcs_attitude_state_t attitude;
    adcs_guidance_target_t target;
    adcs_control_output_t control;
    adcs_health_t health;
} adcs_telemetry_packet_t;

#endif
