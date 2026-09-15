/* Rate-limited quaternion-to-rate and three-axis PID slew controller. */
#ifndef ADCS_CONTROL_SLEW_CONTROLLER_H
#define ADCS_CONTROL_SLEW_CONTROLLER_H

#include <stdint.h>

#include "communication/message.h"
#include "control/control_math.h"

typedef struct {
    float attitude_rate_gain_s;
    adcs_pid_config_t rate_pid;
    float maximum_torque_nm;
    float maximum_rate_rad_s;
    float settled_angle_rad;
    float settled_rate_rad_s;
    uint32_t settled_cycles_required;
} adcs_slew_config_t;

typedef struct {
    adcs_slew_config_t config;
    adcs_pid_state_t axis_pid[ADCS_VECTOR_LENGTH];
    uint32_t settled_cycles;
} adcs_slew_state_t;

/* Loads slew gains and clears the target-settling counter. */
void adcs_slew_init(
    adcs_slew_state_t *state,
    const adcs_slew_config_t *config);

/* Clears mode-local history when a new target or mode is selected. */
void adcs_slew_reset(adcs_slew_state_t *state);

/*
 * Converts shortest-path quaternion error to a rate target, closes three PID
 * rate loops, applies the lower of controller and target rate limits, and
 * declares settled after consecutive in-tolerance cycles.
 */
adcs_result_t adcs_slew_update(
    adcs_slew_state_t *state,
    const adcs_attitude_state_t *attitude,
    const versor target_quaternion,
    float target_maximum_rate_rad_s,
    float dt_s,
    float requested_torque_nm[ADCS_VECTOR_LENGTH],
    uint8_t *is_settled);

#endif
