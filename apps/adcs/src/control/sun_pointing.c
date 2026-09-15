#include "control/sun_pointing.h"

#include <math.h>
#include <string.h>

#include "control/control_math.h"

adcs_result_t adcs_sun_pointing_update(
    const adcs_sun_pointing_config_t *config,
    const adcs_sensor_packet_t *sensors,
    const adcs_attitude_state_t *attitude,
    float requested_torque_nm[ADCS_VECTOR_LENGTH],
    float *pointing_error_rad) {
    float sun_unit[ADCS_VECTOR_LENGTH];
    float axis_unit[ADCS_VECTOR_LENGTH];
    float cross[ADCS_VECTOR_LENGTH];
    float torque[ADCS_VECTOR_LENGTH];
    float dot = 0.0F;

    if (requested_torque_nm != NULL) {
        memset(requested_torque_nm, 0, sizeof(float) * ADCS_VECTOR_LENGTH);
    }
    if (pointing_error_rad != NULL) {
        *pointing_error_rad = 0.0F;
    }
    if (config == NULL || sensors == NULL || attitude == NULL ||
        requested_torque_nm == NULL || pointing_error_rad == NULL ||
        attitude->valid == 0U ||
        (sensors->valid_mask & ADCS_SENSOR_VALID_SUN) == 0U ||
        sensors->sun_irradiance_w_m2 < config->minimum_sun_irradiance_w_m2 ||
        !isfinite(config->proportional_gain_nm) ||
        !isfinite(config->rate_damping_gain_nm_s) ||
        !isfinite(config->maximum_torque_nm) || config->maximum_torque_nm <= 0.0F ||
        !adcs_values_are_finite(attitude->angular_rate_rad_s, ADCS_VECTOR_LENGTH) ||
        adcs_vector_normalize(sensors->sun_vector_body, sun_unit) != ADCS_RESULT_OK ||
        adcs_vector_normalize(config->pointing_axis_body, axis_unit) != ADCS_RESULT_OK) {
        return ADCS_RESULT_UNAVAILABLE;
    }

    adcs_vector_cross(axis_unit, sun_unit, cross);
    for (size_t axis = 0U; axis < ADCS_VECTOR_LENGTH; ++axis) {
        dot += axis_unit[axis] * sun_unit[axis];
        torque[axis] = config->proportional_gain_nm * cross[axis] -
                       config->rate_damping_gain_nm_s *
                           attitude->angular_rate_rad_s[axis];
    }

    *pointing_error_rad = acosf(adcs_clampf(dot, -1.0F, 1.0F));
    return adcs_vector_limit(
        torque,
        config->maximum_torque_nm,
        requested_torque_nm,
        NULL);
}
