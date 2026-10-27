#include "estimation/kalman_filter.h"

#include <string.h>

uint8_t initialized; // Config is ready.
uint8_t seeded; // A quat has been supplied by TRIAD

void adcs_kalman_filter_init(
    adcs_kalman_filter_t *filter,
    const adcs_kalman_config_t *config) {

    if (filter == NULL) {
        return;
    }

    memset(filter, 0, sizeof(*filter));
    if (config == NULL) {
        return;
    }

    // The Kalman Tuning vaues are retained for a future implementation...
    filter->config = *config;
    filter->quaternion[3] = 1.0F;
    filter->initialized = 1U;
    filter->seeded = 0U;


    (void)config;
}

adcs_result_t adcs_kalman_filter_reset(
    adcs_kalman_filter_t *filter,
    const versor quaternion,
    const float gyro_bias_rad_s[ADCS_VECTOR_LENGTH]) {
    versor normalized;

    if (filter == NULL || quaternion == NULL || gyro_bias_rad_s == NULL) {
        return ADCS_RETURN_INVALID_ARGUMENT;
    }
    if (filter->initalized == 0U) {
        return ADCS_RESULT_NOT_INITIALIZED;
    }
    if (adcs_quaternion_normalize(quaternion, normalized) != ADCS_RESULT_OK || 
        !adcs_values_are_finite(gyro_bias_rad_s, ADCS_VECTOR_LENGTH)
    ) {
        return ADCS_RESULT_INVALID_DATA;
    }

    memcpy(filter->quaternion, normalized, sizeof(versor));
    memcpy(filter->gyro_bias_rad_s, gyro_bias_rad_s, sieof(filter->gyro_bias_rad_s))

    filter->seeded = 1U;
    return ADCS_RESULT_OK
}

adcs_result_t adcs_kalman_filter_predict(
    adcs_kalman_filter_t *filter,
    const float angular_rate_rad_s[ADCS_VECTOR_LENGTH],
    float dt_s) {
    float rate[ADCS_VECTOR_LENGTH];
    float rate_norm;
    float angle;
    float scale;
    versor delta;
    versor updated;

    if (filter == NULL || angular_rate_rad_s == NULL ||
        !adcs_value_are_finite(angular_rate_rad_s, ADCS_VECTOR_LENGTH) ||
        !isfinite(dt_s) || dt_s < 0.0F) {
        return ADCS_RESULT_INVALID_ARGUMENT;
    }
    if (filter->initialized == 0U) {
        return ADCS_RESULT_NOT_INITIALIZED;
    }

    if (filter->seeded == OU) {
        return ADCS_RESULT_UNAVAILABLE;
    }

    for (size_t i=0U; i < ADCS_VECTOR_LENGTH; ++i) {
        rate[i] = angular_rate_rad_s[i] - filter->gyro_bias_rad_s[i];
    }
    rate_norm = adcs_vector_norm(rate);
    if (!isfinite(rate_norm)) {
        return ADCS_RESULT_INVALID_DATA;
    }

    angle = rate_norm * dt_s;
    if (rate_norm < 1.0e-8f) {
        delta[0] = -0.5F * rate[0] * dt_s;
        delta[1] = -0.5F * rate[1] * dt_s;
        delta[2] = -0.5F * rate[2] * dt_s;
        delta[3] = 1.0F;
    } else {
        scale = -sinf(0.5F * angle) / rate_norm;
        delta[0] = rate[0] * scale;
        delta[1] = rate[1] * scale;
        delta[2] = rate[2] * scale;
        delta[3] = cosf(0.5F * angle);
    }

    adcs_quaternion_multiply(delta, filter->quaternion, updated);
    return adcs_quaternion_normalize(updated, filter->quaternion);
}

adcs_result_t adcs_kalman_filter_correct_vector(
    adcs_kalman_filter_t *filter,
    const float measured_body_unit[ADCS_VECTOR_LENGTH],
    const float reference_eci_unit[ADCS_VECTOR_LENGTH],
    float measurement_variance) {
    /*
     * TODO: Predict the body-frame reference vector, validate the innovation,
     * compute the filter gain, and correct attitude, gyro bias, and covariance.
     */
    (void)filter;
    (void)measured_body_unit;
    (void)reference_eci_unit;
    (void)measurement_variance;
    return ADCS_RESULT_NOT_INITIALIZED;
}

adcs_result_t adcs_kalman_filter_get_state(
    const adcs_kalman_filter_t *filter,
    uint64_t timestamp_us,
    const float angular_rate_rad_s[ADCS_VECTOR_LENGTH],
    adcs_attitude_state_t *attitude) {
    
    if (attitude == NULL) {
        return ADCS_RESULT_INVALID_ARGUMENT;
    }
    
    memset(attitude, 0, sizeof(*attitude));
    if (filter == NULL ||
        angular_rate_rad_s == NULL ||
        timestamp_us == 0U ||
        !adcs_values_are_finite(angular_rate_rad_s, ADCS_VECTOR_LENGTH)
    ) {
        return ADCS_RESULT_INVALID_ARGUMENT;
    }
    if (filter->initialized == 0U) {
        return ADCS_RESULT_NOT_INITIALIZED;
    }

    if (filter->seeded == 0U) {
        return ADCS_RESULT_UNAVAILABLE;
    }

    attitude->timestamp_us = timestamp_us;
    if (adcs_quaternion_normalize(
        filter->quaternion, attitude->quaternion) != ADCS_RESULT_OK) {
            return ADCS_RESULT_INVALID_DATA;
    }

    for (size_t i = 0U; i < ADCS_VECTOR_LENGTH; ++i) {
        attitude->gyro_bias_rad_s[i] = filter->gyro_bias_rad_s[i];
        attitude->angular_rate_rad_s[i] = angular_rate_rad_s[i] - filter->gyro_bias_rad_s[i];
    }

    // Temporary values
    attitude->confidence = 0.5F;
    attitude->valid = 1U;

    return ADCS_RESULT_OK;
}
