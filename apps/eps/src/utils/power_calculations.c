/*
 * Instant Power calculation
 * Energy is the integral of power over time
 * Average power over a time interval
 * Peak power calculation (max value in power profile)
 * Loss calculation
 * State of charge calculation
 * Battery efficiency calculation
 * etc
 *
 * This is just a few options for functions that are usable. There are alot more.
 * I don't know if battery is gonna provide needed info for this, but if it doesn't we can probably implement all of these.
 * They seem to be general math utils so we can probably just use a library or ai generate one (ofc if so check thoroughly)
*/

/*
instant_power = volts × amperes
energy_joules = instant_power × seconds
energy_watt_hours = energy_joules / 3600
average_power = energy_joules / seconds
peak_power = max of power_samples
loss = power_in − power_out
state_of_charge = remaining_charge / full_capacity
efficiency = power_out / power_in
*/


#include "utils/power_calculations.h"

float eps_power_instant(float volts, float amperes){
    return volts * amperes;
}

float eps_energy_joules(float power_watts, float seconds){
    return power_watts * seconds;
}

float eps_energy_watt_hours(float energy_joules){
    return energy_joules / 3600;
}

float eps_average_power(float energy_joules, float seconds){
    if (seconds == 0) return 0.0F;
    return energy_joules / seconds;
}

float eps_peak_power(const float *power_samples, size_t count){
    if (power_samples == NULL || count == 0){
        return 0.0F;
    }
    float m = power_samples[0];
    for (size_t i = 1; i < count; i++){
        if(m < power_samples[i]) {
            m = power_samples[i];
        } 
    }
    return m;
}

float eps_power_loss(float power_in, float power_out){
    return power_in - power_out;
}

float eps_state_of_charge(float remaining_charge, float full_capacity){
    if (full_capacity == 0) return 0.0F;
    return remaining_charge / full_capacity;
}

float eps_efficiency(float power_in, float power_out){
    if (power_in == 0) return 0.0F;
    return power_out / power_in;
}