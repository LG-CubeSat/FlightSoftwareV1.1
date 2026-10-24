/* Converts requested body torque into a feasible magnetorquer dipole. */
#ifndef ADCS_CONTROL_ACTUATOR_ALLOCATOR_H
#define ADCS_CONTROL_ACTUATOR_ALLOCATOR_H

#include "communication/message.h"

typedef struct {
    float maximum_axis_dipole_a_m2[ADCS_VECTOR_LENGTH];
    float minimum_usable_field_t;
} adcs_actuator_allocator_config_t;

/*
 * Uses m = (B x torque) / |B|^2, then applies per-axis dipole limits.
 * Magnetic actuation cannot create torque parallel to B, so achievable_torque
 * reports the actual perpendicular torque rather than echoing the request.
 */
adcs_result_t adcs_actuator_allocate(
    const adcs_actuator_allocator_config_t *config,
    const float requested_torque_nm[ADCS_VECTOR_LENGTH],
    const float magnetic_field_t[ADCS_VECTOR_LENGTH],
    adcs_magnetorquer_command_t *command,
    float achievable_torque_nm[ADCS_VECTOR_LENGTH],
    uint8_t *was_saturated);

/* Produces an explicitly disabled, zero-dipole command. */
void adcs_actuator_disable(adcs_magnetorquer_command_t *command);

#endif
