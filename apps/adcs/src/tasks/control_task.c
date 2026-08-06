/*
Control Task
runs 20-50 hz
Waits for attitude
Asks Manager
Runs correct controller, controls magnetorquers
*/

/*
control Task
Continously waits for ground messages or messages from other boards
Decodes them and updates manager
*/

#include "../../include/tasks/control_task.h"

#include "FreeRTOS.h"
#include "task.h"

// TODO.. add includes for actually integrating into other tasks

#define CONTROL_TASK_PRIORITY (4)
#define CONTROL_TASK_STACK_SIZE (1024)
#define CONTROL_TASK_PERIOD_MS (50)

static StackType_t xControlTaskStack[CONTROL_TASK_STACK_SIZE];
static StaticTask_t xControlTaskBuffer;

TaskHandle_t xControlHandle = NULL;

void control_task_init(void)
{
    // initialize the task
    xControlHandle = xTaskCreateStatic(
        control_task,
        "control",
        CONTROL_TASK_STACK_SIZE,
        NULL,
        CONTROL_TASK_PRIORITY,
        xControlTaskStack,
        &xControlTaskBuffer
    );

    if (xControlHandle == NULL)
    {
        printf("[CONTROL] Failed to initialize.\n");
    }
}

// superloop of the task
void control_task(void *pvParameters)
{
    (void) pvParameters;

    TickType_t lastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        // do some stuff

        xTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(CONTROL_TASK_PERIOD_MS)
        );
    }
}