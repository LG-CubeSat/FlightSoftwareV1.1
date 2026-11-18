/* Top-level control-law dispatcher for every ADCS mode. */
#ifndef ADCS_CONTROL_CONTROLLER_H
#define ADCS_CONTROL_CONTROLLER_H

#include "communication/message.h"
#include "control/actuator_allocator.h"
#include "control/bdot.h"
#include "control/slew_controller.h"
#include "control/sun_pointing.h"

typedef struct {
    adcs_bdot_config_t bdot;
    adcs_slew_config_t slew;
    adcs_sun_pointing_config_t sun_pointing;
    adcs_actuator_allocator_config_t allocator;
} adcs_controller_config_t;

typedef struct {
    adcs_controller_config_t config;
    adcs_bdot_state_t bdot;
    adcs_slew_state_t slew;
    adcs_mode_t previous_mode;
    uint8_t initialized;
} adcs_controller_t;

/* Initializes every mode controller with one validated configuration. */
void adcs_controller_init(
    adcs_controller_t *controller,
    const adcs_controller_config_t *config);

/* Clears controller histories and returns the dispatcher to safe output. */
void adcs_controller_reset(adcs_controller_t *controller);

/*
 * Runs the controller selected by mode, routes every active-mode torque through
 * the magnetorquer allocator, and fails to explicit safe output.
 */
adcs_result_t adcs_controller_update(
    adcs_controller_t *controller,
    adcs_mode_t mode,
    const adcs_guidance_target_t *target,
    const adcs_sensor_packet_t *sensors,
    const adcs_attitude_state_t *attitude,
    float dt_s,
    adcs_control_output_t *output);

#endif
