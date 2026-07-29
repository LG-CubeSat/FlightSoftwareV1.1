#include <stdio.h>

#include "control_task.h"
#include "control_queue.h"
#include "control_request.h"

#include "adcs_context.h"

/*
Notes
- Determines what to do
- This sets the state and target based on a control request sent to the queue
- Heart of the ADCS
- Reads sensor data
- calculates
- This will command the actuators
*/

void control_task_initialize(void)
{
    printf("[CONTROL TASK] Initialized.\n");
}

// will be periodic
void control_task_run(void)
{
    ControlRequest request;

    if (!control_queue_pop(&request))
    {
        return;
    }


    printf("[CONTROL TASK] Processing request.\n");

    switch (request.type)
    {
        case CONTROL_REQUEST_ENTER_SAFE:
            printf(
                "[CONTROL TASK] Entering SAFE mode.\n"
            );
        
            adcs_context_set_state(
                ADCS_STATE_SAFE
            );

            adcs_context_set_target(
                ADCS_TARGET_NONE
            );

            break;
        
        case CONTROL_REQUEST_EXIT_SAFE:
            printf(
                "[CONTROL TASK] Leaving SAFE mode.\n"
            );

            adcs_context_set_state(
                ADCS_STATE_IDLE
            );

            break;

        case CONTROL_REQUEST_POINT:
            print(
                "[CONTROL TASK] Pointing request recieved"
            );

            adcs_context_set_target(
                request.target
            );

            adcs_context_set_state(
                ADCS_STATE_TRANSITIONING
            );

            break;

        default:

            printf(
                "[CONTROL TASK Unkown request.\n]"
            );

            break;
    }
}