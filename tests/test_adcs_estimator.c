/* Checks the temporary TRIAD + gyro attitude path without starting FreeRTOS. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "estimation/attitude_estimator.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "line %d: %s failed\n", __LINE__, #condition); \
        exit(1); \
    } \
} while (0)

static adcs_attitude_estimator_config_t test_config(void) {
    return (adcs_attitude_estimator_config_t) {
        .kalman = {
            .gyro_noise_rad_s_sqrt_hz = 0.0005F,
            .gyro_bias_walk_rad_s2_sqrt_hz = 0.00001F,
            .initial_attitude_variance_rad2 = 0.05F,
            .initial_bias_variance_rad2_s2 = 0.0001F,
            .minimum_measurement_variance = 1.0e-8F,
            .maximum_innovation = 1.2F
        },
        .magnetometer_measurement_variance = 0.0001F,
        .sun_sensor_measurement_variance = 0.000025F,
        .minimum_magnetic_field_t = 1.0e-6F,
        .maximum_magnetic_field_t = 1.0e-3F,
        .minimum_sun_irradiance_w_m2 = 100.0F,
        .maximum_sample_gap_s = 0.25F,
        .maximum_consecutive_rejections = 100U
    };
}

static void test_filter_seed_and_gyro_direction(void) {
    adcs_kalman_filter_t filter;
    adcs_kalman_config_t config = test_config().kalman;
    adcs_attitude_state_t attitude;
    versor identity = {0.0F, 0.0F, 0.0F, 1.0F};
    const float zero_bias[3] = {0.0F, 0.0F, 0.0F};
    const float gyro[3] = {0.0F, 0.0F, 1.0F};

    adcs_kalman_filter_init(&filter, &config);
    CHECK(filter.initialized != 0U);
    CHECK(adcs_kalman_filter_get_state(&filter, 100000U, gyro, &attitude) ==
          ADCS_RESULT_UNAVAILABLE);
    CHECK(attitude.valid == 0U);

    CHECK(adcs_kalman_filter_reset(&filter, identity, zero_bias) ==
          ADCS_RESULT_OK);
    CHECK(adcs_kalman_filter_predict(&filter, gyro, 0.0F) ==
          ADCS_RESULT_INVALID_ARGUMENT);
    CHECK(adcs_kalman_filter_predict(&filter, gyro, 0.1F) ==
          ADCS_RESULT_OK);
    CHECK(adcs_kalman_filter_get_state(&filter, 100000U, gyro, &attitude) ==
          ADCS_RESULT_OK);
    CHECK(attitude.valid != 0U);
    CHECK(fabsf(attitude.quaternion[2] + sinf(0.05F)) < 0.001F);
    CHECK(fabsf(attitude.quaternion[3] - cosf(0.05F)) < 0.001F);
    CHECK(fabsf(attitude.angular_rate_rad_s[2] - 1.0F) < 0.0001F);
}

static void test_triad_fix_gyro_hold_and_expiry(void) {
    adcs_attitude_estimator_t estimator;
    adcs_attitude_estimator_config_t config = test_config();
    adcs_attitude_state_t attitude;
    adcs_reference_vectors_t references = {
        .sun_eci_unit = {1.0F, 0.0F, 0.0F},
        .magnetic_field_eci_t = {0.0F, 0.0F, 3.0e-5F},
        .valid_mask = ADCS_REFERENCE_VALID_SUN |
                      ADCS_REFERENCE_VALID_MAGNETIC_FIELD
    };
    adcs_sensor_packet_t sensors = {
        .timestamp_us = 10000U,
        .angular_rate_rad_s = {0.0F, 0.0F, 0.0F},
        .magnetic_field_t = {0.0F, 0.0F, 3.0e-5F},
        .sun_vector_body = {1.0F, 0.0F, 0.0F},
        .sun_irradiance_w_m2 = 1000.0F,
        .valid_mask = ADCS_SENSOR_VALID_GYROSCOPE |
                      ADCS_SENSOR_VALID_MAGNETOMETER |
                      ADCS_SENSOR_VALID_SUN
    };

    adcs_attitude_estimator_init(&estimator, &config);
    CHECK(estimator.initialized != 0U);
    CHECK(adcs_attitude_estimator_update(
              &estimator, &sensors, &references, &attitude) == ADCS_RESULT_OK);
    CHECK(attitude.valid != 0U);
    CHECK(attitude.confidence >= 0.35F);
    CHECK(fabsf(attitude.quaternion[3] - 1.0F) < 0.001F);

    /* Lose the Sun: gyro may briefly propagate the last TRIAD fix. */
    sensors.valid_mask &= ~ADCS_SENSOR_VALID_SUN;
    sensors.angular_rate_rad_s[2] = 1.0F;
    sensors.timestamp_us += 10000U;
    CHECK(adcs_attitude_estimator_update(
              &estimator, &sensors, &references, &attitude) == ADCS_RESULT_OK);
    CHECK(attitude.valid != 0U);
    CHECK(attitude.quaternion[2] < 0.0F);

    /* Continue at 100 Hz until the absolute fix is more than one second old. */
    for (unsigned int i = 0U; i < 100U; ++i) {
        sensors.timestamp_us += 10000U;
        (void)adcs_attitude_estimator_update(
            &estimator, &sensors, &references, &attitude);
    }
    CHECK(attitude.valid == 0U);
    CHECK(attitude.confidence == 0.0F);

    /* A fresh pair of vectors should make the estimate usable again. */
    sensors.valid_mask |= ADCS_SENSOR_VALID_SUN;
    sensors.timestamp_us += 10000U;
    CHECK(adcs_attitude_estimator_update(
              &estimator, &sensors, &references, &attitude) == ADCS_RESULT_OK);
    CHECK(attitude.valid != 0U);
}

int main(void) {
    test_filter_seed_and_gyro_direction();
    test_triad_fix_gyro_hold_and_expiry();
    puts("ADCS temporary estimator checks passed");
    return 0;
}
