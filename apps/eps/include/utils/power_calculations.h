/*
 * Power/energy math for the EPS.
 *
 * The accumulator integrates instantaneous power over time so the estimation
 * task can publish energy and average power without keeping its own state.
 * All power is watts, all energy is watt-hours.
 */
#ifndef EPS_UTILS_POWER_CALCULATIONS_H
#define EPS_UTILS_POWER_CALCULATIONS_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    float accumulated_energy_wh;
    float elapsed_time_s;
    uint64_t last_timestamp_us;
    uint8_t initialized;
} eps_energy_accumulator_t;

/* Instantaneous power from a voltage/current pair (P = V * I). */
float eps_instant_power_w(float voltage_v, float current_a);

/* Peak of a sample buffer, or 0 when the buffer is empty/NULL. */
float eps_peak_power_w(const float *samples, size_t count);

/* Loss between an input and output power, clamped at zero. */
float eps_loss_power_w(float input_power_w, float output_power_w);

/* Linear state-of-charge estimate from pack voltage, clamped to [0, 1]. */
float eps_state_of_charge(
    float pack_voltage_v,
    float cell_empty_v,
    float cell_full_v,
    uint8_t cell_count);

/* Round-trip efficiency from discharge and charge energy, in [0, 1]. */
float eps_battery_efficiency(float discharge_energy_wh, float charge_energy_wh);

void eps_energy_accumulator_init(eps_energy_accumulator_t *accumulator);

/* Integrates one sample. Non-positive dt (or a first call) only re-anchors. */
void eps_energy_accumulator_update(
    eps_energy_accumulator_t *accumulator,
    float instant_power_w,
    uint64_t timestamp_us);

float eps_energy_accumulator_energy_wh(
    const eps_energy_accumulator_t *accumulator);

float eps_energy_accumulator_average_power_w(
    const eps_energy_accumulator_t *accumulator);

#endif
