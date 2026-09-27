#include "prop_int_controller.h"

#include <math.h>
#include <stddef.h>

static float controller_proportional_gain = 0.0f;
static float controller_integral_gain = 0.0f;
static float controller_integral_error = 0.0f;

static int controller_initialized = 0;

int prop_int_controller_init(
    float proportional_gain,
    float integral_gain)
{
    if (!isfinite(proportional_gain) ||
        !isfinite(integral_gain) ||
        proportional_gain < 0.0f ||
        integral_gain < 0.0f)
    {
        controller_initialized = 0;
        return 0;
    }

    controller_proportional_gain = proportional_gain;
    controller_integral_gain = integral_gain;
    controller_integral_error = 0.0f;
    controller_initialized = 1;

    return 1;
}

void prop_int_controller_reset(void)
{
    controller_integral_error = 0.0f;
}

int prop_int_controller_update(
    float target_temperature_c,
    float current_temperature_c,
    float elapsed_seconds,
    float *power_fraction_out)
{
    float error_c;
    float proportional_output;
    float candidate_integral_error;
    float candidate_output;

    if (power_fraction_out == NULL)
    {
        return 0;
    }

    *power_fraction_out = 0.0f;

    if (controller_initialized == 0 ||
        !isfinite(target_temperature_c) ||
        !isfinite(current_temperature_c) ||
        !isfinite(elapsed_seconds) ||
        elapsed_seconds <= 0.0f)
    {
        return 0;
    }

    error_c =
        target_temperature_c - current_temperature_c;

    if (error_c <= 0.0f)
    {
        controller_integral_error = 0.0f;
        return 1;
    }

    proportional_output =
        controller_proportional_gain * error_c;

    candidate_integral_error =
        controller_integral_error +
        (error_c * elapsed_seconds);

    candidate_output =
        proportional_output +
        (controller_integral_gain * candidate_integral_error);

    if (candidate_output >= 1.0f)
    {
        *power_fraction_out = 1.0f;
        return 1;
    }

    if (candidate_output <= 0.0f)
    {
        *power_fraction_out = 0.0f;
        return 1;
    }

    controller_integral_error = candidate_integral_error;
    *power_fraction_out = candidate_output;

    return 1;
}