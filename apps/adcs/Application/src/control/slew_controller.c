#include "control/slew_controller.h"

#include <math.h>
#include <string.h>

void adcs_slew_init(
    adcs_slew_state_t *state,
    const adcs_slew_config_t *config) {
    if (state == NULL) {
        return;
    }

    memset(state, 0, sizeof(*state));
    if (config == NULL || !isfinite(config->attitude_rate_gain_s) ||
        !isfinite(config->maximum_torque_nm) ||
        !isfinite(config->maximum_rate_rad_s) ||
        !isfinite(config->settled_angle_rad) ||
        !isfinite(config->settled_rate_rad_s) ||
        config->attitude_rate_gain_s <= 0.0F ||
        config->maximum_torque_nm <= 0.0F ||
        config->maximum_rate_rad_s <= 0.0F ||
        config->settled_angle_rad < 0.0F ||
        config->settled_rate_rad_s < 0.0F ||
        config->settled_cycles_required == 0U) {
        return;
    }

    state->config = *config;
    for (size_t axis = 0U; axis < ADCS_VECTOR_LENGTH; ++axis) {
        adcs_pid_init(&state->axis_pid[axis], &config->rate_pid);
    }
}

void adcs_slew_reset(adcs_slew_state_t *state) {
    if (state == NULL) {
        return;
    }

    state->settled_cycles = 0U;
    for (size_t axis = 0U; axis < ADCS_VECTOR_LENGTH; ++axis) {
        adcs_pid_reset(&state->axis_pid[axis]);
    }
}

adcs_result_t adcs_slew_update(
    adcs_slew_state_t *state,
    const adcs_attitude_state_t *attitude,
    const versor target_quaternion,
    float target_maximum_rate_rad_s,
    float dt_s,
    float requested_torque_nm[ADCS_VECTOR_LENGTH],
    uint8_t *is_settled,
    float *pointing_error_rad,
    uint8_t *was_limited) {
    float attitude_error[ADCS_VECTOR_LENGTH];
    float desired_rate[ADCS_VECTOR_LENGTH];
    float limited_rate[ADCS_VECTOR_LENGTH];
    float torque[ADCS_VECTOR_LENGTH];
    float angle_error;
    float body_rate;
    float effective_rate_limit;

    if (requested_torque_nm != NULL) {
        memset(requested_torque_nm, 0, sizeof(float) * ADCS_VECTOR_LENGTH);
    }
    if (is_settled != NULL) {
        *is_settled = 0U;
    }
    if (pointing_error_rad != NULL) {
        *pointing_error_rad = 0.0F;
    }
    if (was_limited != NULL) {
        *was_limited = 0U;
    }
    if (state == NULL || attitude == NULL || target_quaternion == NULL ||
        requested_torque_nm == NULL || is_settled == NULL ||
        pointing_error_rad == NULL || was_limited == NULL ||
        attitude->valid == 0U || !isfinite(dt_s) || dt_s <= 0.0F ||
        !isfinite(target_maximum_rate_rad_s) ||
        target_maximum_rate_rad_s <= 0.0F ||
        !adcs_values_are_finite(attitude->angular_rate_rad_s, ADCS_VECTOR_LENGTH) ||
        adcs_quaternion_error_vector(
            attitude->quaternion,
            target_quaternion,
            attitude_error) != ADCS_RESULT_OK) {
        return ADCS_RESULT_INVALID_DATA;
    }

    for (size_t axis = 0U; axis < ADCS_VECTOR_LENGTH; ++axis) {
        desired_rate[axis] = state->config.attitude_rate_gain_s *
                             attitude_error[axis];
    }
    effective_rate_limit = fminf(
        state->config.maximum_rate_rad_s,
        target_maximum_rate_rad_s);
    if (adcs_vector_limit(
            desired_rate,
            effective_rate_limit,
            limited_rate,
            NULL) != ADCS_RESULT_OK) {
        return ADCS_RESULT_INVALID_DATA;
    }

    for (size_t axis = 0U; axis < ADCS_VECTOR_LENGTH; ++axis) {
        float rate_error = limited_rate[axis] - attitude->angular_rate_rad_s[axis];
        if (adcs_pid_update(
                &state->axis_pid[axis],
                rate_error,
                dt_s,
                &torque[axis]) != ADCS_RESULT_OK) {
            return ADCS_RESULT_INVALID_DATA;
        }
    }
    if (adcs_vector_limit(
            torque,
            state->config.maximum_torque_nm,
            requested_torque_nm,
            was_limited) != ADCS_RESULT_OK) {
        return ADCS_RESULT_INVALID_DATA;
    }

    angle_error = adcs_vector_norm(attitude_error);
    body_rate = adcs_vector_norm(attitude->angular_rate_rad_s);
    *pointing_error_rad = angle_error;
    if (angle_error <= state->config.settled_angle_rad &&
        body_rate <= state->config.settled_rate_rad_s) {
        if (state->settled_cycles < state->config.settled_cycles_required) {
            ++state->settled_cycles;
        }
    } else {
        state->settled_cycles = 0U;
    }
    *is_settled = state->settled_cycles >= state->config.settled_cycles_required;
    return ADCS_RESULT_OK;
}
