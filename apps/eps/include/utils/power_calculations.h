// See: powercalculations.c

#ifndef POWER_CALCULATIONS_H
#define POWER_CALCULATIONS_H

#include <stddef.h>

float eps_power_instant(float volts, float amperes);
float eps_energy_joules(float power_watts, float seconds);
float eps_energy_watt_hours(float energy_joules);
float eps_average_power(float energy_joules, float seconds);
float eps_peak_power(const float *power_samples, size_t count);
float eps_power_loss(float power_in, float power_out);
float eps_state_of_charge(float remaining_charge, float full_capacity);
float eps_efficiency(float power_in, float power_out);

#endif