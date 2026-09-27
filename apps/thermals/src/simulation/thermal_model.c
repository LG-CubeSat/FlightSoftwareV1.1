/*
 * Simulation-only single-bed thermal environment.
 *
 * TODO(SIM): replace these placeholder physical values with measured or
 * estimated values for the final thermal-bed design.
 * TODO(HW): do not compile this model into the STM32 hardware target.
 */

#include "simulation/thermal_model.h"

#include <math.h>
#include <pthread.h>

#define SIM_INITIAL_TEMPERATURE_C       (30.0f)
#define SIM_AMBIENT_TEMPERATURE_C       (20.0f)
#define SIM_MAX_HEATER_POWER_W          (5.0f)
#define SIM_THERMAL_CAPACITY_J_PER_C    (100.0f)
#define SIM_HEAT_LOSS_W_PER_C           (0.2f)
#define SIM_MAX_STEP_SECONDS            (1.0f)

static pthread_mutex_t thermal_model_lock = PTHREAD_MUTEX_INITIALIZER;

static float model_temperature_c = SIM_INITIAL_TEMPERATURE_C;

static int model_initialized = 0;

int thermal_model_init(void)
{
    pthread_mutex_lock(&thermal_model_lock);

    model_temperature_c = SIM_INITIAL_TEMPERATURE_C;
    model_initialized = 1;

    pthread_mutex_unlock(&thermal_model_lock);

    return 1;
}

int thermal_model_step(
    float heater_power_fraction,
    float elapsed_seconds)
{
    float heater_power_w;
    float heat_loss_w;
    float net_power_w;
    float temperature_change_c;

    if (!isfinite(heater_power_fraction) || !isfinite(elapsed_seconds) ||
        heater_power_fraction < 0.0f || heater_power_fraction > 1.0f ||
        elapsed_seconds <= 0.0f || elapsed_seconds > SIM_MAX_STEP_SECONDS)
    {
        return 0;
    }

    pthread_mutex_lock(&thermal_model_lock);

    if (model_initialized == 0)
    {
        pthread_mutex_unlock(&thermal_model_lock);
        return 0;
    }

    heater_power_w = heater_power_fraction * SIM_MAX_HEATER_POWER_W;

    heat_loss_w = SIM_HEAT_LOSS_W_PER_C * (model_temperature_c - SIM_AMBIENT_TEMPERATURE_C);

    net_power_w = heater_power_w - heat_loss_w;

    temperature_change_c = (net_power_w / SIM_THERMAL_CAPACITY_J_PER_C) * elapsed_seconds;

    model_temperature_c += temperature_change_c;

    pthread_mutex_unlock(&thermal_model_lock);

    return 1;
}

float thermal_model_get_temperature(void)
{
    float temperature_c;

    pthread_mutex_lock(&thermal_model_lock);

    temperature_c = model_temperature_c;

    pthread_mutex_unlock(&thermal_model_lock);

    return temperature_c;
}