#include "control/control_math.h"

#include <math.h>
#include <string.h>

#define ADCS_MATH_EPSILON 1.0e-9F

static uint8_t pid_config_is_valid(const adcs_pid_config_t *config) {
    return config != NULL &&
           isfinite(config->proportional_gain) &&
           isfinite(config->integral_gain) &&
           isfinite(config->derivative_gain) &&
           isfinite(config->integrator_limit) &&
           isfinite(config->output_limit) &&
           config->proportional_gain >= 0.0F &&
           config->integral_gain >= 0.0F &&
           config->derivative_gain >= 0.0F &&
           config->integrator_limit >= 0.0F &&
           config->output_limit > 0.0F;
}

static void quaternion_multiply(const versor left, const versor right, versor output) {
    versor value;

    value[0] = left[3] * right[0] + left[0] * right[3] +
               left[1] * right[2] - left[2] * right[1];
    value[1] = left[3] * right[1] - left[0] * right[2] +
               left[1] * right[3] + left[2] * right[0];
    value[2] = left[3] * right[2] + left[0] * right[1] -
               left[1] * right[0] + left[2] * right[3];
    value[3] = left[3] * right[3] - left[0] * right[0] -
               left[1] * right[1] - left[2] * right[2];
    memcpy(output, value, sizeof(versor));
}

void adcs_pid_init(adcs_pid_state_t *state, const adcs_pid_config_t *config) {
    if (state == NULL) {
        return;
    }

    memset(state, 0, sizeof(*state));
    if (pid_config_is_valid(config)) {
        state->config = *config;
    }
}

void adcs_pid_reset(adcs_pid_state_t *state) {
    if (state == NULL) {
        return;
    }

    state->integral = 0.0F;
    state->previous_error = 0.0F;
    state->has_previous_error = 0U;
}

adcs_result_t adcs_pid_update(
    adcs_pid_state_t *state,
    float error,
    float dt_s,
    float *output) {
    float derivative = 0.0F;
    float candidate_integral;
    float raw_output;

    if (state == NULL || output == NULL || !isfinite(error) ||
        !isfinite(dt_s) || dt_s <= 0.0F ||
        !pid_config_is_valid(&state->config)) {
        if (output != NULL) {
            *output = 0.0F;
        }
        return ADCS_RESULT_INVALID_ARGUMENT;
    }

    candidate_integral = state->integral + error * dt_s;
    state->integral = adcs_clampf(
        candidate_integral,
        -state->config.integrator_limit,
        state->config.integrator_limit);

    if (state->has_previous_error != 0U) {
        derivative = (error - state->previous_error) / dt_s;
    }

    raw_output = state->config.proportional_gain * error +
                 state->config.integral_gain * state->integral +
                 state->config.derivative_gain * derivative;
    *output = adcs_clampf(
        raw_output,
        -state->config.output_limit,
        state->config.output_limit);

    if (state->config.integral_gain > ADCS_MATH_EPSILON &&
        raw_output != *output) {
        state->integral -= (raw_output - *output) /
                           state->config.integral_gain;
        state->integral = adcs_clampf(
            state->integral,
            -state->config.integrator_limit,
            state->config.integrator_limit);
    }

    state->previous_error = error;
    state->has_previous_error = 1U;
    return ADCS_RESULT_OK;
}

float adcs_clampf(float value, float minimum, float maximum) {
    if (isnan(value) || isnan(minimum) || isnan(maximum) || minimum > maximum) {
        return value;
    }
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

float adcs_vector_norm(const float vector[ADCS_VECTOR_LENGTH]) {
    if (vector == NULL || !adcs_values_are_finite(vector, ADCS_VECTOR_LENGTH)) {
        return NAN;
    }

    return sqrtf(vector[0] * vector[0] +
                 vector[1] * vector[1] +
                 vector[2] * vector[2]);
}

adcs_result_t adcs_vector_normalize(
    const float input[ADCS_VECTOR_LENGTH],
    float output[ADCS_VECTOR_LENGTH]) {
    float normalized[ADCS_VECTOR_LENGTH];
    float norm;

    if (input == NULL || output == NULL) {
        return ADCS_RESULT_INVALID_ARGUMENT;
    }

    norm = adcs_vector_norm(input);
    if (!isfinite(norm) || norm < ADCS_MATH_EPSILON) {
        memset(output, 0, sizeof(normalized));
        return ADCS_RESULT_INVALID_DATA;
    }

    for (size_t axis = 0U; axis < ADCS_VECTOR_LENGTH; ++axis) {
        normalized[axis] = input[axis] / norm;
    }
    memcpy(output, normalized, sizeof(normalized));
    return ADCS_RESULT_OK;
}

void adcs_vector_cross(
    const float left[ADCS_VECTOR_LENGTH],
    const float right[ADCS_VECTOR_LENGTH],
    float output[ADCS_VECTOR_LENGTH]) {
    float cross[ADCS_VECTOR_LENGTH] = {0.0F, 0.0F, 0.0F};

    if (left == NULL || right == NULL || output == NULL) {
        return;
    }

    cross[0] = left[1] * right[2] - left[2] * right[1];
    cross[1] = left[2] * right[0] - left[0] * right[2];
    cross[2] = left[0] * right[1] - left[1] * right[0];
    memcpy(output, cross, sizeof(cross));
}

adcs_result_t adcs_vector_limit(
    const float input[ADCS_VECTOR_LENGTH],
    float limit,
    float output[ADCS_VECTOR_LENGTH],
    uint8_t *was_limited) {
    float norm;
    float scale = 1.0F;

    if (input == NULL || output == NULL || !isfinite(limit) || limit < 0.0F ||
        !adcs_values_are_finite(input, ADCS_VECTOR_LENGTH)) {
        if (output != NULL) {
            memset(output, 0, sizeof(float) * ADCS_VECTOR_LENGTH);
        }
        if (was_limited != NULL) {
            *was_limited = 0U;
        }
        return ADCS_RESULT_INVALID_ARGUMENT;
    }

    norm = adcs_vector_norm(input);
    if (norm > limit && norm > ADCS_MATH_EPSILON) {
        scale = limit / norm;
    }

    for (size_t axis = 0U; axis < ADCS_VECTOR_LENGTH; ++axis) {
        output[axis] = input[axis] * scale;
    }
    if (was_limited != NULL) {
        *was_limited = scale < 1.0F ? 1U : 0U;
    }
    return ADCS_RESULT_OK;
}

adcs_result_t adcs_quaternion_normalize(const versor input, versor output) {
    float norm_squared = 0.0F;
    float inverse_norm;
    versor normalized;

    if (input == NULL || output == NULL || !adcs_values_are_finite(input, 4U)) {
        return ADCS_RESULT_INVALID_ARGUMENT;
    }

    for (size_t index = 0U; index < 4U; ++index) {
        norm_squared += input[index] * input[index];
    }
    if (!isfinite(norm_squared) || norm_squared < ADCS_MATH_EPSILON) {
        return ADCS_RESULT_INVALID_DATA;
    }

    inverse_norm = 1.0F / sqrtf(norm_squared);
    for (size_t index = 0U; index < 4U; ++index) {
        normalized[index] = input[index] * inverse_norm;
    }

    if (normalized[3] < 0.0F) {
        for (size_t index = 0U; index < 4U; ++index) {
            normalized[index] = -normalized[index];
        }
    }
    memcpy(output, normalized, sizeof(versor));
    return ADCS_RESULT_OK;
}

adcs_result_t adcs_quaternion_error_vector(
    const versor current,
    const versor target,
    float error[ADCS_VECTOR_LENGTH]) {
    versor current_unit;
    versor target_unit;
    versor current_conjugate;
    versor mapping_error;
    float vector_norm;
    float angle;

    if (error == NULL ||
        adcs_quaternion_normalize(current, current_unit) != ADCS_RESULT_OK ||
        adcs_quaternion_normalize(target, target_unit) != ADCS_RESULT_OK) {
        if (error != NULL) {
            memset(error, 0, sizeof(float) * ADCS_VECTOR_LENGTH);
        }
        return ADCS_RESULT_INVALID_DATA;
    }

    current_conjugate[0] = -current_unit[0];
    current_conjugate[1] = -current_unit[1];
    current_conjugate[2] = -current_unit[2];
    current_conjugate[3] = current_unit[3];
    quaternion_multiply(target_unit, current_conjugate, mapping_error);

    if (mapping_error[3] < 0.0F) {
        for (size_t index = 0U; index < 4U; ++index) {
            mapping_error[index] = -mapping_error[index];
        }
    }

    vector_norm = sqrtf(mapping_error[0] * mapping_error[0] +
                        mapping_error[1] * mapping_error[1] +
                        mapping_error[2] * mapping_error[2]);
    if (vector_norm < ADCS_MATH_EPSILON) {
        memset(error, 0, sizeof(float) * ADCS_VECTOR_LENGTH);
        return ADCS_RESULT_OK;
    }

    angle = 2.0F * atan2f(vector_norm, adcs_clampf(mapping_error[3], 0.0F, 1.0F));
    for (size_t axis = 0U; axis < ADCS_VECTOR_LENGTH; ++axis) {
        error[axis] = -mapping_error[axis] * angle / vector_norm;
    }
    return ADCS_RESULT_OK;
}

uint8_t adcs_values_are_finite(const float *values, size_t count) {
    if (values == NULL) {
        return 0U;
    }

    for (size_t index = 0U; index < count; ++index) {
        if (!isfinite(values[index])) {
            return 0U;
        }
    }
    return 1U;
}
