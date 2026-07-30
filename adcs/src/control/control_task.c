#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

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

static StaticTask_t xControlTaskBuffer; // TCB (holds priortiy state etc)
static StackType_t xControlStack[configMINIMAL_STACK_SIZE]; // local variable storage

void control_task_initialize(void)
{
    printf("[CONTROL TASK] Initialized.\n");
}

void control_task_loop(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(20); // 50Hz

    printf("[CONTROL TASK] Task Started at 50Hz.\n");

    for (;;)
    {
        control_task_run();
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// will be periodic
void control_task_run(void)
{
    ControlRequest request;

    if (control_queue_pop(&request))
    {
        // if an event occurs then handle it
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
                printf(
                    "[CONTROL TASK] Pointing request recieved\n"
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
                    "[CONTROL TASK] Unknown request.\n"
                );

                break;
        }
        printf("[CONTROL TASK] Processing request complete.\n");
    }

    // Deterministic Control.
    // Must run every single time control task run is called
    // In a real satellite, this is where sensors are read and PID is run.
    static int cycle_count = 0;
    if (cycle_count++ % 50 == 0) { // Print once per second at 50Hz
        printf("[CONTROL TASK] Heartbeat - State: %d\n", adcs_context_get().state);
    }
}

// task function to call in main
TaskHandle_t control_task_create_static(void)
{
    return xTaskCreateStatic(
        control_task_loop, // The function we wrote to earlier
        "CONTROL", // name for debugging
        configMINIMAL_STACK_SIZE,
        NULL, // No parameters needed
        2, // Priority (Higher than Command)
        xControlStack, // The stack we just defined
        &xControlTaskBuffer // The TCB we just defined
    );
}