#include "tasks/command_task.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "thermal_data.h"
#include "tasks/thermal_write_task.h"

#include <stdio.h>

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
        return 0;
    }

    return xQueueSendFromISR(
        xCommandQueue,
        message,
        NULL
    ) == pdPASS;
}

void command_task_init(void)
{
    xCommandQueue = xQueueCreateStatic(
        COMMAND_QUEUE_LENGTH,
        sizeof(CommandMessage_t),
        xCommandQueueStorage,
        &xCommandQueueBuffer
    );

    if (xCommandQueue == NULL)
    {
        printf("[THERMAL_COMMAND] Queue creation failed.\n");
        return;
    }

    xCommandHandle = xTaskCreateStatic(
        command_task,
        "thermal_cmd",
        COMMAND_TASK_STACK_SIZE,
        NULL,
        COMMAND_TASK_PRIORITY,
        xCommandTaskStack,
        &xCommandTaskBuffer
    );

    if (xCommandHandle == NULL)
    {
        printf("[THERMAL_COMMAND] Task creation failed.\n");
        return;
    }
}

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
            int32_t command = (int32_t)message.command;


            printf("[COMMAND] Dispatching temperature command: target=%d\n", goal_temp);
            fflush(stdout);

            

            // send responses -- Telemetry reports the new position back to
            // the OBC once it wakes up and processes the notification above
        }
    }
}

