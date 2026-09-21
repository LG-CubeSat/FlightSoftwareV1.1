/* Generates quaternion targets for built-in and commanded pointing modes. */
#ifndef ADCS_GUIDANCE_TARGET_GENERATOR_H
#define ADCS_GUIDANCE_TARGET_GENERATOR_H

#include "communication/message.h"

/* Builds an ECI-to-body attitude that maps inertial_direction to body_axis. */
adcs_result_t adcs_guidance_target_from_vector(
    adcs_mode_t mode,
    const float body_axis[ADCS_VECTOR_LENGTH],
    const float inertial_direction[ADCS_VECTOR_LENGTH],
    float maximum_rate_rad_s,
    adcs_guidance_target_t *target);

/* Validates and normalizes an explicitly commanded ECI-to-body attitude. */
adcs_result_t adcs_guidance_target_from_attitude(
    adcs_mode_t mode,
    const versor target_quaternion,
    float maximum_rate_rad_s,
    adcs_guidance_target_t *target);

#endif
