#ifndef THERMALS_SIMULATION_THERMAL_MODEL_H
#define THERMALS_SIMULATION_THERMAL_MODEL_H

int thermal_model_init(void);

int thermal_model_step(
    float heater_power_fraction,
    float elapsed_seconds
);

float thermal_model_get_temperature(void);

#endif // THERMALS_SIMULATION_THERMAL_MODEL_H