/* Coarse sun-vector pointing controller. */
#ifndef ADCS_CONTROL_SUN_POINTING_H
#define ADCS_CONTROL_SUN_POINTING_H

#include "communication/message.h"

typedef struct {
    float proportional_gain_nm;
    float rate_damping_gain_nm_s;
    float maximum_torque_nm;
    float minimum_sun_irradiance_w_m2;
    float pointing_axis_body[ADCS_VECTOR_LENGTH];
} adcs_sun_pointing_config_t;

/*
 * Uses the cross product between the configured spacecraft pointing axis and
 * measured body-frame sun vector, with angular-rate damping, to request body
 * torque. Eclipse or invalid sun data produces an unavailable result.
 */
adcs_result_t adcs_sun_pointing_update(
    const adcs_sun_pointing_config_t *config,
    const adcs_sensor_packet_t *sensors,
    const adcs_attitude_state_t *attitude,
    float requested_torque_nm[ADCS_VECTOR_LENGTH],
    float *pointing_error_rad,
    uint8_t *was_limited);

#endif
