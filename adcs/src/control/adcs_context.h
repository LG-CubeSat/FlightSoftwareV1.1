#ifndef ADCS_CONTEXT_H
#define ADCS_CONTEXT_H

typedef enum
{
    ADCS_STATE_SAFE = 0,
    ADCS_STATE_IDLE,
    ADCS_STATE_POINTING,
    ADCS_STATE_TRANSITIONING
} AdcsState;

typedef enum
{
    ADCS_TARGET_NONE = 0,
    ADCS_TARGET_SUN,
    ADCS_TARGET_EARTH,
    ADCS_TARGET_NADIR
} AdcsTarget;

typedef struct
{
    AdcsState state;

    AdcsTarget target;

    float current_angle;
    float target_angle;
} AdcsContext;

void adcs_context_initialize(void);

// this is where the context gets fetched (i.e sensor data, current state, current target)
AdcsContext adcs_context_get(void);

void adcs_context_set_state(
    AdcsState state
);

AdcsState adcs_context_get_state();

void adcs_context_set_target(
    AdcsTarget target
);

AdcsTarget adcs_context_get_target();

float adcs_context_get_current_angle(void);

void adcs_context_set_current_angle(
    float angle
);

float adcs_context_get_target_angle(void);

void adcs_context_set_target_angle(
    float angle
);

#endif