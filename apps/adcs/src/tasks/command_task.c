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
#include "../../../../libs/libcsp/include/csp/csp_types.h"

// TODO.. add includes for actually integrating into other tasks

#define COMMAND_TASK_PRIORITY (3)
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

int command_task_send(csp_packet_t *command)
{
    if (xCommandQueue == NULL)
    {
        return 0; // failed
    }

    return xQueueSend(
        xCommandQueue,
        command,
        0
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

            // update managers

            // send responses
        }
    }
}