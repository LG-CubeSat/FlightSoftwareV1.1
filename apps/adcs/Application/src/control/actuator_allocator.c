#include "control/actuator_allocator.h"

#include <math.h>
#include <string.h>

#include "control/control_math.h"

adcs_result_t adcs_actuator_allocate(
    const adcs_actuator_allocator_config_t *config,
    const float requested_torque_nm[ADCS_VECTOR_LENGTH],
    const float magnetic_field_t[ADCS_VECTOR_LENGTH],
    adcs_magnetorquer_command_t *command,
    float achievable_torque_nm[ADCS_VECTOR_LENGTH],
    uint8_t *was_saturated) {
    float field_cross_torque[ADCS_VECTOR_LENGTH];
    float field_norm;
    float field_squared;

    if (command != NULL) {
        adcs_actuator_disable(command);
    }
    if (achievable_torque_nm != NULL) {
        memset(achievable_torque_nm, 0, sizeof(float) * ADCS_VECTOR_LENGTH);
    }
    if (was_saturated != NULL) {
        *was_saturated = 0U;
    }
    if (config == NULL || requested_torque_nm == NULL ||
        magnetic_field_t == NULL || command == NULL ||
        achievable_torque_nm == NULL || was_saturated == NULL ||
        !adcs_values_are_finite(requested_torque_nm, ADCS_VECTOR_LENGTH) ||
        !adcs_values_are_finite(magnetic_field_t, ADCS_VECTOR_LENGTH) ||
        !adcs_values_are_finite(config->maximum_axis_dipole_a_m2, ADCS_VECTOR_LENGTH) ||
        !isfinite(config->minimum_usable_field_t) ||
        config->minimum_usable_field_t <= 0.0F) {
        return ADCS_RESULT_INVALID_ARGUMENT;
    }

    field_norm = adcs_vector_norm(magnetic_field_t);
    if (!isfinite(field_norm) || field_norm < config->minimum_usable_field_t) {
        return ADCS_RESULT_UNAVAILABLE;
    }
    field_squared = field_norm * field_norm;
    adcs_vector_cross(magnetic_field_t, requested_torque_nm, field_cross_torque);

    for (size_t axis = 0U; axis < ADCS_VECTOR_LENGTH; ++axis) {
        float limit = config->maximum_axis_dipole_a_m2[axis];
        float requested_dipole;

        if (limit <= 0.0F) {
            adcs_actuator_disable(command);
            return ADCS_RESULT_INVALID_ARGUMENT;
        }
        requested_dipole = field_cross_torque[axis] / field_squared;
        command->dipole_a_m2[axis] = adcs_clampf(requested_dipole, -limit, limit);
        if (command->dipole_a_m2[axis] != requested_dipole) {
            *was_saturated = 1U;
        }
    }

    adcs_vector_cross(command->dipole_a_m2, magnetic_field_t, achievable_torque_nm);
    command->enabled = 1U;
    return ADCS_RESULT_OK;
}

void adcs_actuator_disable(adcs_magnetorquer_command_t *command) {
    if (command == NULL) {
        return;
    }

    memset(command, 0, sizeof(*command));
}
