#include "guidance/target_generator.h"

#include <math.h>
#include <string.h>

#include "control/control_math.h"

#define GUIDANCE_OPPOSITE_VECTOR_EPSILON 1.0e-5F

static adcs_result_t quaternion_between_vectors(
    const float from[ADCS_VECTOR_LENGTH],
    const float to[ADCS_VECTOR_LENGTH],
    versor quaternion) {
    float from_unit[ADCS_VECTOR_LENGTH];
    float to_unit[ADCS_VECTOR_LENGTH];
    float cross[ADCS_VECTOR_LENGTH];
    float dot = 0.0F;
    versor raw;

    if (adcs_vector_normalize(from, from_unit) != ADCS_RESULT_OK ||
        adcs_vector_normalize(to, to_unit) != ADCS_RESULT_OK) {
        return ADCS_RESULT_INVALID_DATA;
    }

    for (size_t axis = 0U; axis < ADCS_VECTOR_LENGTH; ++axis) {
        dot += from_unit[axis] * to_unit[axis];
    }
    dot = adcs_clampf(dot, -1.0F, 1.0F);

    if (dot > 1.0F - GUIDANCE_OPPOSITE_VECTOR_EPSILON) {
        raw[0] = 0.0F;
        raw[1] = 0.0F;
        raw[2] = 0.0F;
        raw[3] = 1.0F;
    } else if (dot < -1.0F + GUIDANCE_OPPOSITE_VECTOR_EPSILON) {
        float basis[ADCS_VECTOR_LENGTH] = {1.0F, 0.0F, 0.0F};
        if (fabsf(from_unit[0]) > 0.8F) {
            basis[0] = 0.0F;
            basis[1] = 1.0F;
        }
        adcs_vector_cross(from_unit, basis, cross);
        if (adcs_vector_normalize(cross, cross) != ADCS_RESULT_OK) {
            return ADCS_RESULT_INVALID_DATA;
        }
        raw[0] = cross[0];
        raw[1] = cross[1];
        raw[2] = cross[2];
        raw[3] = 0.0F;
    } else {
        adcs_vector_cross(from_unit, to_unit, cross);
        raw[0] = cross[0];
        raw[1] = cross[1];
        raw[2] = cross[2];
        raw[3] = 1.0F + dot;
    }

    return adcs_quaternion_normalize(raw, quaternion);
}

adcs_result_t adcs_guidance_target_from_vector(
    adcs_mode_t mode,
    const float body_axis[ADCS_VECTOR_LENGTH],
    const float inertial_direction[ADCS_VECTOR_LENGTH],
    float maximum_rate_rad_s,
    adcs_guidance_target_t *target) {
    adcs_guidance_target_t generated;

    if (body_axis == NULL || inertial_direction == NULL || target == NULL ||
        mode < ADCS_MODE_BOOT || mode >= ADCS_MODE_COUNT ||
        !isfinite(maximum_rate_rad_s) || maximum_rate_rad_s <= 0.0F) {
        return ADCS_RESULT_INVALID_ARGUMENT;
    }

    memset(&generated, 0, sizeof(generated));
    if (adcs_vector_normalize(
            body_axis,
            generated.pointing_axis_body) != ADCS_RESULT_OK ||
        adcs_vector_normalize(
            inertial_direction,
            generated.target_direction_eci) != ADCS_RESULT_OK ||
        quaternion_between_vectors(
            generated.target_direction_eci,
            generated.pointing_axis_body,
            generated.target_quaternion) != ADCS_RESULT_OK) {
        return ADCS_RESULT_INVALID_DATA;
    }

    generated.mode = mode;
    generated.maximum_rate_rad_s = maximum_rate_rad_s;
    *target = generated;
    return ADCS_RESULT_OK;
}

adcs_result_t adcs_guidance_target_from_attitude(
    adcs_mode_t mode,
    const versor target_quaternion,
    float maximum_rate_rad_s,
    adcs_guidance_target_t *target) {
    adcs_guidance_target_t generated;

    if (target_quaternion == NULL || target == NULL ||
        mode < ADCS_MODE_BOOT || mode >= ADCS_MODE_COUNT ||
        !isfinite(maximum_rate_rad_s) || maximum_rate_rad_s <= 0.0F) {
        return ADCS_RESULT_INVALID_ARGUMENT;
    }

    memset(&generated, 0, sizeof(generated));
    if (adcs_quaternion_normalize(
            target_quaternion,
            generated.target_quaternion) != ADCS_RESULT_OK) {
        return ADCS_RESULT_INVALID_DATA;
    }
    generated.mode = mode;
    generated.pointing_axis_body[0] = 1.0F;
    generated.maximum_rate_rad_s = maximum_rate_rad_s;
    *target = generated;
    return ADCS_RESULT_OK;
}
