#ifndef THERMALS_PROP_INT_CONTROLLER_H
#define THERMALS_PROP_INT_CONTROLLER_H

int prop_int_controller_init(
    float proportional_gain,
    float integral_gain
);

//Clears accumulated integral error while keeping the configured gains.

void prop_int_controller_reset(void);

/*
 * Calculates normalized heater power from 0.0f to 1.0f.
 *
 * Returns 1 on success.
 * Returns 0 and writes 0.0f to power_fraction_out on failure.
 */
int prop_int_controller_update(
    float target_temperature_c,
    float current_temperature_c,
    float elapsed_seconds,
    float *power_fraction_out
);

#endif // THERMALS_PROP_INT_CONTROLLER_H