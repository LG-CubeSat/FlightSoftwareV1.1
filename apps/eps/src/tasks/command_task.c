/*
Command Task
Continously waits for ground messages or messages from other boards
Decodes them and updates manager
*/

#include "../../include/tasks/command_task.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"

#include <stdio.h>

#include "../../include/tasks/control_task.h"
#include "../../include/tasks/estimation_task.h"
#include "../../include/tasks/sensor_task.h"
#include "../../include/tasks/telemetry_task.h"

#define COMMAND_TASK_PRIORITY (2)
#define COMMAND_TASK_STACK_SIZE (1024)
#define COMMAND_QUEUE_LENGTH (8)

static StackType_t xCommandTaskStack[COMMAND_TASK_STACK_SIZE];
static StaticTask_t xCommandTaskBuffer;

static StaticQueue_t xCommandQueueBuffer;
static uint8_t xCommandQueueStorage[
    COMMAND_QUEUE_LENGTH * sizeof(CommandMessage_t)
];

static TaskHandle_t xCommandHandle = NULL;
static QueueHandle_t xCommandQueue = NULL;

int command_task_send(const CommandMessage_t *message)
{
    if (xCommandQueue == NULL)
    {
        return 0; // failed
    }

    /* command_task_send() is called from command_handler.c's rx thread,
     * which is a plain POSIX pthread, not a FreeRTOS task -- the regular
     * xQueueSend() assumes a task context (a valid pxCurrentTCB) and hangs
     * when called from a foreign thread. The FromISR variant is the
     * documented way to feed a FreeRTOS queue from any non-task context. */
    return xQueueSendFromISR(
        xCommandQueue,
        message,
        NULL
    ) == pdPASS;
}

void command_task_init(void)
{
    // initalize the queue
    xCommandQueue = xQueueCreateStatic(
        COMMAND_QUEUE_LENGTH,
        sizeof(CommandMessage_t),
        xCommandQueueStorage,
        &xCommandQueueBuffer
    );

    if (xCommandQueue == NULL)
    {
        printf("[COMMAND] Queue creation failed.\n");
        return;
    }
    
    // initialize the task
    xCommandHandle = xTaskCreateStatic(
        command_task,
        "Command",
        COMMAND_TASK_STACK_SIZE,
        NULL,
        COMMAND_TASK_PRIORITY,
        xCommandTaskStack,
        &xCommandTaskBuffer
    );

    if (xCommandHandle == NULL)
    {
        printf("[COMMAND] Task Creation failed.\n");
        return;
    }
}

// superloop of the task
void command_task(void *pvParameters)
{
    (void) pvParameters;

    CommandMessage_t message;

    for (;;)
    {
        if (xQueueReceive(
            xCommandQueue,
            &message,
            portMAX_DELAY))
        {
            // decode command
            int32_t target_position = (int32_t)message.parameter;
            printf("[COMMAND] Dispatching position command: target=%d\n", target_position);
            fflush(stdout);

            // update managers -- notify every task the position command
            // affects (Housekeeping is intentionally excluded)
            xTaskNotify(xControlHandle, (uint32_t)target_position, eSetValueWithOverwrite);
            xTaskNotify(xEstimationHandle, (uint32_t)target_position, eSetValueWithOverwrite);
            xTaskNotify(xSensorHandle, (uint32_t)target_position, eSetValueWithOverwrite);
            xTaskNotify(xTelemetryHandle, (uint32_t)target_position, eSetValueWithOverwrite);

            // send responses -- Telemetry reports the new position back to
            // the OBC once it wakes up and processes the notification above
        }
    }
}