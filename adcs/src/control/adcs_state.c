#include <stdio.h>

#include "adcs_state.h"

static AdcsState current_state;

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

void adcs_state_initialize(void)
{
    current_state = ADCS_STATE_SAFE;
    printf("[ADCS STATE] Initialized State: %s.\n", adcs_state_name(current_state));
}

AdcsState adcs_get_state()
{
    return current_state; 
}

// here is where the logic handling occurs
int adcs_state_request(
    AdcsState requested_state
)
{
    printf(
        "[ADCS STATE] Transition requested: %s -> %s\n",
        adcs_state_name(current_state), 
        adcs_state_name(requested_state)
    );

    if (current_state == requested_state)
    {
        printf(
            "[ADCS STATE] Already in requested state.\n"
        );
        return 1;
    }

    /*
    SAFE -> IDLE
    */

    if (
        current_state == ADCS_STATE_SAFE && requested_state == ADCS_STATE_IDLE
    )
    {
        current_state = ADCS_STATE_IDLE;

        printf(
            "[ADCS STATE] Transition successful.\n"
        );

        return 1;
    }

    /*
    IDLE -> Pointing
    */
    if (
        current_state == ADCS_STATE_IDLE && requested_state == ADCS_STATE_POINTING
    )
    {
        current_state = ADCS_STATE_TRANSITIONING; // moving toward new target

        printf(
            "[ADCS STATE] Beginning transition to POINTING.\n"
        );

        return 1;
    }

    /*
    ANY -> SAFE (Emergency)
    */
    if (
        requested_state == ADCS_STATE_SAFE
    )
    {
        current_state = ADCS_STATE_SAFE;
        
        printf(
            "[ADCS STATE] Emergency transition to SAFE.\n"
        );

        return 1;
    }

    printf(
        "[ADCS STATE] Transition rejected.\n"
    );

    return 0;
}

void adcs_state_update(void)
{
    if (current_state == ADCS_STATE_TRANSITIONING)
    {
        printf(
            "[ADCS STATE] Transitioning...\n"
        );

        /*TODO: I will have this function check sensors.
        Once spacecraft is stable, transition to POINTING*/

        current_state = ADCS_STATE_POINTING;

        printf(
            "[ADCS STATE] Now POINTING.\n"
        );
    }
}