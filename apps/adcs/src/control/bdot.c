#include "control/bdot.h"

#include <math.h>
#include <string.h>

#include "control/control_math.h"

void adcs_bdot_init(
    adcs_bdot_state_t *state,
    const adcs_bdot_config_t *config) {
    if (state == NULL) {
        return;
    }

    memset(state, 0, sizeof(*state));
    if (config != NULL && isfinite(config->gain_a_m2_s_t) &&
        isfinite(config->derivative_filter_alpha) &&
        isfinite(config->maximum_dipole_a_m2) &&
        isfinite(config->minimum_dt_s) && isfinite(config->maximum_dt_s) &&
        config->gain_a_m2_s_t >= 0.0F &&
        config->derivative_filter_alpha >= 0.0F &&
        config->derivative_filter_alpha <= 1.0F &&
        config->maximum_dipole_a_m2 > 0.0F &&
        config->minimum_dt_s > 0.0F &&
        config->maximum_dt_s >= config->minimum_dt_s) {
        state->config = *config;
    }
}

void adcs_bdot_reset(adcs_bdot_state_t *state) {
    if (state == NULL) {
        return;
    }

    memset(state->previous_field_t, 0, sizeof(state->previous_field_t));
    memset(state->filtered_derivative_t_s, 0, sizeof(state->filtered_derivative_t_s));
    state->has_previous_sample = 0U;
}

adcs_result_t adcs_bdot_update(
    adcs_bdot_state_t *state,
    const float magnetic_field_t[ADCS_VECTOR_LENGTH],
    float dt_s,
    float dipole_a_m2[ADCS_VECTOR_LENGTH],
    uint8_t *was_limited) {
    float raw_derivative[ADCS_VECTOR_LENGTH];
    float requested[ADCS_VECTOR_LENGTH];

    if (dipole_a_m2 != NULL) {
        memset(dipole_a_m2, 0, sizeof(float) * ADCS_VECTOR_LENGTH);
    }
    if (was_limited != NULL) {
        *was_limited = 0U;
    }
    if (state == NULL || magnetic_field_t == NULL || dipole_a_m2 == NULL ||
        was_limited == NULL ||
        !adcs_values_are_finite(magnetic_field_t, ADCS_VECTOR_LENGTH) ||
        !isfinite(dt_s) || dt_s < state->config.minimum_dt_s ||
        dt_s > state->config.maximum_dt_s ||
        state->config.maximum_dipole_a_m2 <= 0.0F) {
        return ADCS_RESULT_INVALID_ARGUMENT;
    }

    if (state->has_previous_sample == 0U) {
        memcpy(state->previous_field_t, magnetic_field_t, sizeof(state->previous_field_t));
        state->has_previous_sample = 1U;
        return ADCS_RESULT_OK;
    }

    for (size_t axis = 0U; axis < ADCS_VECTOR_LENGTH; ++axis) {
        raw_derivative[axis] =
            (magnetic_field_t[axis] - state->previous_field_t[axis]) / dt_s;
        state->filtered_derivative_t_s[axis] =
            state->config.derivative_filter_alpha * raw_derivative[axis] +
            (1.0F - state->config.derivative_filter_alpha) *
                state->filtered_derivative_t_s[axis];
        requested[axis] = -state->config.gain_a_m2_s_t *
                          state->filtered_derivative_t_s[axis];
    }

    memcpy(state->previous_field_t, magnetic_field_t, sizeof(state->previous_field_t));
    return adcs_vector_limit(
        requested,
        state->config.maximum_dipole_a_m2,
        dipole_a_m2,
        was_limited);
}
