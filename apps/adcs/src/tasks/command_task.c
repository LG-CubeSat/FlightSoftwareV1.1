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
#define COMMAND_TASK_PERIOD_MS (20)
#define COMMAND_TASK_QUEUE_LENGTH (8)
#define COMMAND_TASK_ITEM_SIZE sizeof(uint32_t)

static StackType_t xCommandTaskStack[COMMAND_TASK_STACK_SIZE];
static StaticTask_t xCommandTaskBuffer;
static StaticQueue_t xCommandStaticQueue;
static uint8_t commandQueueStorageArea[COMMAND_TASK_QUEUE_LENGTH * COMMAND_TASK_ITEM_SIZE];

static TaskHandle_t xCommandHandle = NULL;
static QueueHandle_t xCommandQueue = NULL;

void command_task_init(void)
{
    // initalize the queue
    xCommandQueue = xQueueCreateStatic(
        COMMAND_TASK_QUEUE_LENGTH,
        COMMAND_TASK_ITEM_SIZE,
        commandQueueStorageArea,
        &xCommandStaticQueue
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

// superloop of the task
void command_task(void *pvParameters)
{
    (void) pvParameters;

    /* 
    TODO: instead of recieving a packet we should recieve an internal command id not packet
    This means using a custom packet class because technically the packet should be parsed way earlier by telem
    */
    csp_packet_t command;

    for (;;)
    {
        if (xQueueReceive(
            xCommandQueue,
            &command,
            portMAX_DELAY))
        {
            // Decode command
            command_process(&command); // place holder for making command actually do something
        }
    }
}