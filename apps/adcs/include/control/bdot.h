/* B-dot magnetic detumbling controller. */
#ifndef ADCS_CONTROL_BDOT_H
#define ADCS_CONTROL_BDOT_H

#include <stdint.h>

#include "communication/message.h"

typedef struct {
    float gain_a_m2_s_t;
    float derivative_filter_alpha;
    float maximum_dipole_a_m2;
    float minimum_dt_s;
    float maximum_dt_s;
} adcs_bdot_config_t;

typedef struct {
    adcs_bdot_config_t config;
    float previous_field_t[ADCS_VECTOR_LENGTH];
    float filtered_derivative_t_s[ADCS_VECTOR_LENGTH];
    uint8_t has_previous_sample;
} adcs_bdot_state_t;

/* Loads detumble gains and clears magnetic-field history. */
void adcs_bdot_init(
    adcs_bdot_state_t *state,
    const adcs_bdot_config_t *config);

/* Clears field history after a mode change, sensor gap, or time discontinuity. */
void adcs_bdot_reset(adcs_bdot_state_t *state);

/*
 * Estimates dB/dt, filters sensor noise, and commands dipole opposite dB/dt.
 * The first valid sample primes the differentiator and requests zero dipole.
 */
adcs_result_t adcs_bdot_update(
    adcs_bdot_state_t *state,
    const float magnetic_field_t[ADCS_VECTOR_LENGTH],
    float dt_s,
    float dipole_a_m2[ADCS_VECTOR_LENGTH]);

#endif
