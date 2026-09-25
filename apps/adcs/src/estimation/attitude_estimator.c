#include "estimation/attitude_estimator.h"

#include <math.h>
#include <string.h>

#include "control/control_math.h"

static void rotation_matrix_to_quaternion(float matrix[3][3], versor quaternion) {
    float trace = matrix[0][0] + matrix[1][1] + matrix[2][2];

    if (trace > 0.0F) {
        float scale = sqrtf(trace + 1.0F) * 2.0F;
        quaternion[3] = 0.25F * scale;
        quaternion[0] = (matrix[2][1] - matrix[1][2]) / scale;
        quaternion[1] = (matrix[0][2] - matrix[2][0]) / scale;
        quaternion[2] = (matrix[1][0] - matrix[0][1]) / scale;
    } else if (matrix[0][0] > matrix[1][1] && matrix[0][0] > matrix[2][2]) {
        float scale = sqrtf(1.0F + matrix[0][0] - matrix[1][1] - matrix[2][2]) * 2.0F;
        quaternion[3] = (matrix[2][1] - matrix[1][2]) / scale;
        quaternion[0] = 0.25F * scale;
        quaternion[1] = (matrix[0][1] + matrix[1][0]) / scale;
        quaternion[2] = (matrix[0][2] + matrix[2][0]) / scale;
    } else if (matrix[1][1] > matrix[2][2]) {
        float scale = sqrtf(1.0F + matrix[1][1] - matrix[0][0] - matrix[2][2]) * 2.0F;
        quaternion[3] = (matrix[0][2] - matrix[2][0]) / scale;
        quaternion[0] = (matrix[0][1] + matrix[1][0]) / scale;
        quaternion[1] = 0.25F * scale;
        quaternion[2] = (matrix[1][2] + matrix[2][1]) / scale;
    } else {
        float scale = sqrtf(1.0F + matrix[2][2] - matrix[0][0] - matrix[1][1]) * 2.0F;
        quaternion[3] = (matrix[1][0] - matrix[0][1]) / scale;
        quaternion[0] = (matrix[0][2] + matrix[2][0]) / scale;
        quaternion[1] = (matrix[1][2] + matrix[2][1]) / scale;
        quaternion[2] = 0.25F * scale;
    }
    (void)adcs_quaternion_normalize(quaternion, quaternion);
}

static uint8_t filter_state_is_corrupt(
    const adcs_kalman_filter_t *filter) {
    float quaternion_norm_squared = 0.0F;

    if (filter == NULL || filter->initialized == 0U ||
        !adcs_values_are_finite(filter->quaternion, 4U) ||
        !adcs_values_are_finite(
            filter->gyro_bias_rad_s,
            ADCS_VECTOR_LENGTH)) {
        return 1U;
    }
    for (size_t index = 0U; index < 4U; ++index) {
        quaternion_norm_squared +=
            filter->quaternion[index] * filter->quaternion[index];
    }
    if (!isfinite(quaternion_norm_squared) ||
        quaternion_norm_squared < 0.25F ||
        quaternion_norm_squared > 2.25F) {
        return 1U;
    }
    for (size_t index = 0U; index < ADCS_ERROR_STATE_LENGTH; ++index) {
        float diagonal = filter->covariance[
            index * ADCS_ERROR_STATE_LENGTH + index];
        if (!isfinite(diagonal) || diagonal < 0.0F || diagonal > 1.0e6F) {
            return 1U;
        }
    }
    return 0U;
}

static adcs_result_t triad_attitude(
    const float first_body[ADCS_VECTOR_LENGTH],
    const float second_body[ADCS_VECTOR_LENGTH],
    const float first_eci[ADCS_VECTOR_LENGTH],
    const float second_eci[ADCS_VECTOR_LENGTH],
    versor quaternion) {
    float body_basis[3][3];
    float eci_basis[3][3];
    float matrix[3][3];

    if (adcs_vector_normalize(first_body, body_basis[0]) != ADCS_RESULT_OK ||
        adcs_vector_normalize(first_eci, eci_basis[0]) != ADCS_RESULT_OK) {
        return ADCS_RESULT_INVALID_DATA;
    }
    adcs_vector_cross(body_basis[0], second_body, body_basis[1]);
    adcs_vector_cross(eci_basis[0], second_eci, eci_basis[1]);
    if (adcs_vector_normalize(body_basis[1], body_basis[1]) != ADCS_RESULT_OK ||
        adcs_vector_normalize(eci_basis[1], eci_basis[1]) != ADCS_RESULT_OK) {
        return ADCS_RESULT_INVALID_DATA;
    }
    adcs_vector_cross(body_basis[0], body_basis[1], body_basis[2]);
    adcs_vector_cross(eci_basis[0], eci_basis[1], eci_basis[2]);

    for (size_t row = 0U; row < 3U; ++row) {
        for (size_t column = 0U; column < 3U; ++column) {
            matrix[row][column] = 0.0F;
            for (size_t basis = 0U; basis < 3U; ++basis) {
                matrix[row][column] +=
                    body_basis[basis][row] * eci_basis[basis][column];
            }
        }
    }
    rotation_matrix_to_quaternion(matrix, quaternion);
    return ADCS_RESULT_OK;
}

void adcs_attitude_estimator_init(
    adcs_attitude_estimator_t *estimator,
    const adcs_attitude_estimator_config_t *config) {
    if (estimator == NULL) {
        return;
    }

    memset(estimator, 0, sizeof(*estimator));
    if (config == NULL || !isfinite(config->magnetometer_measurement_variance) ||
        !isfinite(config->sun_sensor_measurement_variance) ||
        !isfinite(config->minimum_magnetic_field_t) ||
        !isfinite(config->maximum_magnetic_field_t) ||
        !isfinite(config->minimum_sun_irradiance_w_m2) ||
        !isfinite(config->maximum_sample_gap_s) ||
        config->magnetometer_measurement_variance <= 0.0F ||
        config->sun_sensor_measurement_variance <= 0.0F ||
        config->minimum_magnetic_field_t <= 0.0F ||
        config->maximum_magnetic_field_t <= config->minimum_magnetic_field_t ||
        config->maximum_sample_gap_s <= 0.0F ||
        config->maximum_consecutive_rejections == 0U) {
        return;
    }

    estimator->config = *config;
    adcs_kalman_filter_init(&estimator->filter, &config->kalman);
    estimator->initialized = estimator->filter.initialized;
}

adcs_result_t adcs_attitude_estimator_reset(
    adcs_attitude_estimator_t *estimator,
    const versor quaternion) {
    const float zero_bias[ADCS_VECTOR_LENGTH] = {0.0F, 0.0F, 0.0F};
    adcs_result_t result;

    if (estimator == NULL || estimator->initialized == 0U) {
        return ADCS_RESULT_NOT_INITIALIZED;
    }

    result = adcs_kalman_filter_reset(&estimator->filter, quaternion, zero_bias);
    if (result == ADCS_RESULT_OK) {
        estimator->previous_timestamp_us = 0U;
        estimator->accepted_corrections = 0U;
        estimator->consecutive_rejected_updates = 0U;
        estimator->has_absolute_solution = 0U;
    }
    return result;
}

adcs_result_t adcs_attitude_estimator_update(
    adcs_attitude_estimator_t *estimator,
    const adcs_sensor_packet_t *sensors,
    const adcs_reference_vectors_t *references,
    adcs_attitude_state_t *attitude) {
    uint8_t magnetic_available;
    uint8_t sun_available;
    uint8_t correction_accepted = 0U;
    float magnetic_norm;
    float dt_s;
    adcs_result_t result;

    if (attitude != NULL) {
        memset(attitude, 0, sizeof(*attitude));
    }
    if (estimator == NULL || sensors == NULL || references == NULL ||
        attitude == NULL || estimator->initialized == 0U ||
        sensors->timestamp_us == 0U ||
        (sensors->valid_mask & ADCS_SENSOR_VALID_GYROSCOPE) == 0U ||
        !adcs_values_are_finite(sensors->angular_rate_rad_s, ADCS_VECTOR_LENGTH)) {
        return ADCS_RESULT_INVALID_ARGUMENT;
    }
    if (estimator->previous_timestamp_us != 0U &&
        sensors->timestamp_us <= estimator->previous_timestamp_us) {
        return ADCS_RESULT_INVALID_DATA;
    }

    magnetic_norm = adcs_vector_norm(sensors->magnetic_field_t);
    magnetic_available =
        (sensors->valid_mask & ADCS_SENSOR_VALID_MAGNETOMETER) != 0U &&
        (references->valid_mask & ADCS_REFERENCE_VALID_MAGNETIC_FIELD) != 0U &&
        isfinite(magnetic_norm) &&
        magnetic_norm >= estimator->config.minimum_magnetic_field_t &&
        magnetic_norm <= estimator->config.maximum_magnetic_field_t;
    sun_available =
        (sensors->valid_mask & ADCS_SENSOR_VALID_SUN) != 0U &&
        (references->valid_mask & ADCS_REFERENCE_VALID_SUN) != 0U &&
        sensors->sun_irradiance_w_m2 >=
            estimator->config.minimum_sun_irradiance_w_m2;

    if (estimator->has_absolute_solution == 0U &&
        magnetic_available != 0U && sun_available != 0U) {
        versor initial_attitude;
        const float zero_bias[ADCS_VECTOR_LENGTH] = {0.0F, 0.0F, 0.0F};

        if (triad_attitude(
                sensors->sun_vector_body,
                sensors->magnetic_field_t,
                references->sun_eci_unit,
                references->magnetic_field_eci_t,
                initial_attitude) == ADCS_RESULT_OK &&
            adcs_kalman_filter_reset(
                &estimator->filter,
                initial_attitude,
                zero_bias) == ADCS_RESULT_OK) {
            estimator->has_absolute_solution = 1U;
        }
    }

    if (estimator->previous_timestamp_us != 0U) {
        dt_s = (float)(sensors->timestamp_us - estimator->previous_timestamp_us) /
               1000000.0F;
        if (dt_s > estimator->config.maximum_sample_gap_s) {
            estimator->previous_timestamp_us = sensors->timestamp_us;
            return ADCS_RESULT_INVALID_DATA;
        }
        result = adcs_kalman_filter_predict(
            &estimator->filter,
            sensors->angular_rate_rad_s,
            dt_s);
        if (result != ADCS_RESULT_OK) {
            /* Consume the sample even when propagation rejects it. Otherwise
               the same bad interval is retried forever with an ever-growing
               dt and subsequent good samples are discarded too. */
            estimator->previous_timestamp_us = sensors->timestamp_us;
            return filter_state_is_corrupt(&estimator->filter) != 0U
                ? ADCS_RESULT_OUT_OF_RANGE
                : result;
        }
    }
    estimator->previous_timestamp_us = sensors->timestamp_us;

    if (magnetic_available != 0U) {
        result = adcs_kalman_filter_correct_vector(
            &estimator->filter,
            sensors->magnetic_field_t,
            references->magnetic_field_eci_t,
            estimator->config.magnetometer_measurement_variance);
        if (result == ADCS_RESULT_OK) {
            correction_accepted = 1U;
            ++estimator->accepted_corrections;
        }
    }
    if (sun_available != 0U) {
        result = adcs_kalman_filter_correct_vector(
            &estimator->filter,
            sensors->sun_vector_body,
            references->sun_eci_unit,
            estimator->config.sun_sensor_measurement_variance);
        if (result == ADCS_RESULT_OK) {
            correction_accepted = 1U;
            ++estimator->accepted_corrections;
        }
    }

    if (correction_accepted != 0U) {
        estimator->consecutive_rejected_updates = 0U;
    } else if (estimator->consecutive_rejected_updates < UINT32_MAX) {
        ++estimator->consecutive_rejected_updates;
    }

    result = adcs_kalman_filter_get_state(
        &estimator->filter,
        sensors->timestamp_us,
        sensors->angular_rate_rad_s,
        attitude);
    if (result != ADCS_RESULT_OK) {
        return filter_state_is_corrupt(&estimator->filter) != 0U
            ? ADCS_RESULT_OUT_OF_RANGE
            : result;
    }

    if (estimator->has_absolute_solution == 0U) {
        attitude->confidence *= 0.25F;
    }
    if (estimator->consecutive_rejected_updates >
        estimator->config.maximum_consecutive_rejections) {
        attitude->confidence *= 0.25F;
        attitude->valid = 0U;
        return ADCS_RESULT_INVALID_DATA;
    }
    return ADCS_RESULT_OK;
}
