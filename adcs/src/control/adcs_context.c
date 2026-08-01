#include <stdio.h>

#include "adcs_context.h"

static AdcsContext context;

void adcs_context_initialize(void)
{
    context.state = ADCS_STATE_SAFE;

    context.target = ADCS_TARGET_NONE;
    context.current_angle = 0.0f;
    context.target_angle = 0.0f;

    printf("[ADCS CONTEXT] Initialized.\n");
}

AdcsContext adcs_context_get(void)
{
    return context;
}

void adcs_context_set_state(
    AdcsState state
)
{
    context.state = state;
}

AdcsState adcs_context_get_state(void) {
    return context.state;
}

void adcs_context_set_target(
    AdcsTarget target
)
{
    context.target = target;
}

AdcsTarget adcs_context_get_target(void)
{
    return context.target;
}

float adcs_context_get_current_angle(void)
{
    return context.current_angle;
}

void adcs_context_set_current_angle(float angle)
{
    context.current_angle = angle;
}

float adcs_context_get_target_angle(void)
{
    return context.target_angle;
}

void adcs_context_set_target_angle(float target_angle)
{
    context.target_angle = target_angle;
}

static const char *adcs_state_name(
    AdcsState state
) {
    switch (state)
    {
        case ADCS_STATE_SAFE:
            return "SAFE";
        case ADCS_STATE_IDLE:
            return "IDLE";
        case ADCS_STATE_TRANSITIONING:
            return "TRANSITIONING";
        case ADCS_STATE_POINTING:
            return "POINTING";
        default:
            return "UNKNOWN";
    }
}