#include "estimation/kalman_filter.h"

#include <string.h>

void adcs_kalman_filter_init(
    adcs_kalman_filter_t *filter,
    const adcs_kalman_config_t *config) {
    /*
     * TODO: Validate the filter configuration, initialize the attitude and
     * gyro-bias states, initialize the covariance, and mark the filter ready.
     */
    if (filter != NULL) {
        memset(filter, 0, sizeof(*filter));
    }
    (void)config;
}

adcs_result_t adcs_kalman_filter_reset(
    adcs_kalman_filter_t *filter,
    const versor quaternion,
    const float gyro_bias_rad_s[ADCS_VECTOR_LENGTH]) {
    /*
     * TODO: Validate and store the supplied attitude and gyro-bias seed, reset
     * the covariance, and leave the filter ready for propagation.
     */
    (void)filter;
    (void)quaternion;
    (void)gyro_bias_rad_s;
    return ADCS_RESULT_NOT_INITIALIZED;
}

adcs_result_t adcs_kalman_filter_predict(
    adcs_kalman_filter_t *filter,
    const float angular_rate_rad_s[ADCS_VECTOR_LENGTH],
    float dt_s) {
    /*
     * TODO: Remove gyro bias, propagate the ECI-to-body quaternion through the
     * time step, and propagate the attitude/bias error covariance.
     */
    (void)filter;
    (void)angular_rate_rad_s;
    (void)dt_s;
    return ADCS_RESULT_NOT_INITIALIZED;
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
    /*
     * TODO: Copy the normalized attitude, bias-corrected angular rate,
     * covariance diagonal, validity, and confidence into the output state.
     */
    if (attitude != NULL) {
        memset(attitude, 0, sizeof(*attitude));
    }
    (void)filter;
    (void)timestamp_us;
    (void)angular_rate_rad_s;
    return ADCS_RESULT_NOT_INITIALIZED;
}
