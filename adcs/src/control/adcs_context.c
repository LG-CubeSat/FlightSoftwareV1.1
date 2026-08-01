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

void adcs_context_set_target(
    AdcsTarget target
)
{
    context.target = target;
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