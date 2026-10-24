/* Lightweight multiplicative attitude filter state and API for simulation. */
#ifndef ADCS_ESTIMATION_KALMAN_FILTER_H
#define ADCS_ESTIMATION_KALMAN_FILTER_H

#include <stdint.h>

#include "communication/message.h"

typedef struct {
    float gyro_noise_rad_s_sqrt_hz;
    float gyro_bias_walk_rad_s2_sqrt_hz;
    float initial_attitude_variance_rad2;
    float initial_bias_variance_rad2_s2;
    float minimum_measurement_variance;
    float maximum_innovation;
} adcs_kalman_config_t;

typedef struct {
    adcs_kalman_config_t config;
    versor quaternion;
    float gyro_bias_rad_s[ADCS_VECTOR_LENGTH];
    float covariance[ADCS_COVARIANCE_ELEMENT_COUNT];
    uint8_t initialized;
} adcs_kalman_filter_t;

/* Creates an identity-attitude filter with configured initial covariance. */
void adcs_kalman_filter_init(
    adcs_kalman_filter_t *filter,
    const adcs_kalman_config_t *config);

/* Re-seeds attitude and gyro bias after startup or estimator recovery. */
adcs_result_t adcs_kalman_filter_reset(
    adcs_kalman_filter_t *filter,
    const versor quaternion,
    const float gyro_bias_rad_s[ADCS_VECTOR_LENGTH]);

/*
 * Propagates quaternion and a conservative 6-state covariance approximation
 * with a bias-corrected gyroscope sample over one positive time step.
 */
adcs_result_t adcs_kalman_filter_predict(
    adcs_kalman_filter_t *filter,
    const float angular_rate_rad_s[ADCS_VECTOR_LENGTH],
    float dt_s);

/*
 * Corrects attitude from a normalized body-vector observation and its known
 * inertial reference. The same update supports sun and magnetic vectors with
 * sensor-specific measurement variance and a bounded complementary gain.
 */
adcs_result_t adcs_kalman_filter_correct_vector(
    adcs_kalman_filter_t *filter,
    const float measured_body_unit[ADCS_VECTOR_LENGTH],
    const float reference_eci_unit[ADCS_VECTOR_LENGTH],
    float measurement_variance);

/* Copies the normalized estimate and covariance diagonal into task output. */
adcs_result_t adcs_kalman_filter_get_state(
    const adcs_kalman_filter_t *filter,
    uint64_t timestamp_us,
    const float angular_rate_rad_s[ADCS_VECTOR_LENGTH],
    adcs_attitude_state_t *attitude);

#endif
