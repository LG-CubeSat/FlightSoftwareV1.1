#ifndef ADCS_STATE_H
#define ADCS_STATE_H

typedef enum
{
    ADCS_STATE_SAFE = 0,
    ADCS_STATE_IDLE,
    ADCS_STATE_POINTING,
    ADCS_STATE_TRANSITIONING
} AdcsState;

void adcs_state_initialize(void);

void adcs_state_update(void);

AdcsState adcs_get_state(void);

int adcs_state_request(
    AdcsState request_state
);

#endif