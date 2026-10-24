/* High-level attitude estimator coordinating propagation and corrections. */
#ifndef ADCS_ESTIMATION_ATTITUDE_ESTIMATOR_H
#define ADCS_ESTIMATION_ATTITUDE_ESTIMATOR_H

#include <stdint.h>

#include "communication/message.h"
#include "estimation/kalman_filter.h"

typedef struct {
    adcs_kalman_config_t kalman;
    float magnetometer_measurement_variance;
    float sun_sensor_measurement_variance;
    float minimum_magnetic_field_t;
    float maximum_magnetic_field_t;
    float minimum_sun_irradiance_w_m2;
    float maximum_sample_gap_s;
    uint32_t maximum_consecutive_rejections;
} adcs_attitude_estimator_config_t;

typedef struct {
    adcs_attitude_estimator_config_t config;
    adcs_kalman_filter_t filter;
    uint64_t previous_timestamp_us;
    uint32_t accepted_corrections;
    uint32_t consecutive_rejected_updates;
    uint8_t has_absolute_solution;
    uint8_t initialized;
} adcs_attitude_estimator_t;

/* Initializes estimator configuration and its underlying Kalman filter. */
void adcs_attitude_estimator_init(
    adcs_attitude_estimator_t *estimator,
    const adcs_attitude_estimator_config_t *config);

/* Re-seeds the estimator and clears timestamp and rejection history. */
adcs_result_t adcs_attitude_estimator_reset(
    adcs_attitude_estimator_t *estimator,
    const versor quaternion);

/*
 * Propagates with gyro data, applies valid sun and magnetic reference-vector
 * corrections, rejects stale/outlying samples, and publishes confidence.
 */
adcs_result_t adcs_attitude_estimator_update(
    adcs_attitude_estimator_t *estimator,
    const adcs_sensor_packet_t *sensors,
    const adcs_reference_vectors_t *references,
    adcs_attitude_state_t *attitude);

#endif
