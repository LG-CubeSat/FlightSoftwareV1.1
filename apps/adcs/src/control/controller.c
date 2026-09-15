#include "control/controller.h"

#include <math.h>
#include <string.h>

#include "control/control_math.h"

static void control_output_clear(
    adcs_control_output_t *output,
    uint64_t timestamp_us) {
    memset(output, 0, sizeof(*output));
    output->timestamp_us = timestamp_us;
}

void adcs_controller_init(
    adcs_controller_t *controller,
    const adcs_controller_config_t *config) {
    if (controller == NULL) {
        return;
    }

    memset(controller, 0, sizeof(*controller));
    if (config == NULL) {
        return;
    }

    controller->config = *config;
    adcs_bdot_init(&controller->bdot, &config->bdot);
    adcs_slew_init(&controller->slew, &config->slew);
    controller->previous_mode = ADCS_MODE_BOOT;
    controller->initialized = 1U;
}

void adcs_controller_reset(adcs_controller_t *controller) {
    if (controller == NULL) {
        return;
    }

    adcs_bdot_reset(&controller->bdot);
    adcs_slew_reset(&controller->slew);
    controller->previous_mode = ADCS_MODE_BOOT;
}

adcs_result_t adcs_controller_update(
    adcs_controller_t *controller,
    adcs_mode_t mode,
    const adcs_guidance_target_t *target,
    const adcs_sensor_packet_t *sensors,
    const adcs_attitude_state_t *attitude,
    float dt_s,
    adcs_control_output_t *output) {
    adcs_magnetorquer_command_t actuator;
    float requested_torque[ADCS_VECTOR_LENGTH] = {0.0F, 0.0F, 0.0F};
    float achievable_torque[ADCS_VECTOR_LENGTH] = {0.0F, 0.0F, 0.0F};
    float dipole[ADCS_VECTOR_LENGTH] = {0.0F, 0.0F, 0.0F};
    uint8_t saturated = 0U;
    uint8_t settled = 0U;
    adcs_result_t result;

    if (output == NULL) {
        return ADCS_RESULT_INVALID_ARGUMENT;
    }
    control_output_clear(output, sensors != NULL ? sensors->timestamp_us : 0U);
    if (controller == NULL || sensors == NULL || attitude == NULL ||
        controller->initialized == 0U || mode < ADCS_MODE_BOOT ||
        mode >= ADCS_MODE_COUNT || !isfinite(dt_s) || dt_s <= 0.0F) {
        return ADCS_RESULT_INVALID_ARGUMENT;
    }

    if (mode != controller->previous_mode) {
        adcs_bdot_reset(&controller->bdot);
        adcs_slew_reset(&controller->slew);
        controller->previous_mode = mode;
    }

    if (mode == ADCS_MODE_BOOT || mode == ADCS_MODE_SAFE) {
        return ADCS_RESULT_OK;
    }
    if ((mode == ADCS_MODE_DETUMBLE || mode == ADCS_MODE_SUN_ACQUISITION) &&
        (sensors->valid_mask & ADCS_SENSOR_VALID_MAGNETOMETER) == 0U) {
        return ADCS_RESULT_UNAVAILABLE;
    }

    if (mode == ADCS_MODE_DETUMBLE) {
        result = adcs_bdot_update(
            &controller->bdot,
            sensors->magnetic_field_t,
            dt_s,
            dipole);
        if (result != ADCS_RESULT_OK) {
            return result;
        }

        memcpy(output->requested_dipole_a_m2, dipole, sizeof(dipole));
        adcs_vector_cross(dipole, sensors->magnetic_field_t, achievable_torque);
        memcpy(output->achievable_torque_nm, achievable_torque, sizeof(achievable_torque));
        output->actuators_enabled = 1U;
        return ADCS_RESULT_OK;
    }

    if (mode == ADCS_MODE_SUN_ACQUISITION || mode == ADCS_MODE_SUN_POINTING) {
        result = adcs_sun_pointing_update(
            &controller->config.sun_pointing,
            sensors,
            attitude,
            requested_torque,
            &output->pointing_error_rad);
    } else if (mode == ADCS_MODE_SLEWING ||
               mode == ADCS_MODE_TARGET_POINTING ||
               mode == ADCS_MODE_EARTH_POINTING ||
               mode == ADCS_MODE_SCIENCE) {
        if (target == NULL) {
            return ADCS_RESULT_INVALID_ARGUMENT;
        }
        result = adcs_slew_update(
            &controller->slew,
            attitude,
            target->target_quaternion,
            target->maximum_rate_rad_s,
            dt_s,
            requested_torque,
            &settled);
        if (result == ADCS_RESULT_OK) {
            float error[ADCS_VECTOR_LENGTH];
            if (adcs_quaternion_error_vector(
                    attitude->quaternion,
                    target->target_quaternion,
                    error) == ADCS_RESULT_OK) {
                output->pointing_error_rad = adcs_vector_norm(error);
            }
        }
    } else {
        return ADCS_RESULT_INVALID_ARGUMENT;
    }

    if (result != ADCS_RESULT_OK) {
        return result;
    }

    if (mode != ADCS_MODE_SUN_ACQUISITION) {
        memcpy(output->requested_torque_nm, requested_torque, sizeof(requested_torque));
        memcpy(
            output->reaction_wheel_torque_nm,
            requested_torque,
            sizeof(requested_torque));
        memcpy(
            output->achievable_torque_nm,
            requested_torque,
            sizeof(requested_torque));
        output->actuators_enabled = 1U;
        output->reaction_wheels_enabled = 1U;
        output->target_settled = settled;
        return ADCS_RESULT_OK;
    }

    result = adcs_actuator_allocate(
        &controller->config.allocator,
        requested_torque,
        sensors->magnetic_field_t,
        &actuator,
        achievable_torque,
        &saturated);
    if (result != ADCS_RESULT_OK) {
        return result;
    }

    memcpy(output->requested_torque_nm, requested_torque, sizeof(requested_torque));
    memcpy(output->requested_dipole_a_m2, actuator.dipole_a_m2, sizeof(actuator.dipole_a_m2));
    memcpy(output->achievable_torque_nm, achievable_torque, sizeof(achievable_torque));
    output->actuators_enabled = actuator.enabled;
    output->saturated = saturated;
    output->target_settled = settled;
    return ADCS_RESULT_OK;
}
