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

// TODO.. add includes for actually integrating into other tasks

#define COMMAND_TASK_PRIORITY (3)
#define COMMAND_TASK_STACK_SIZE (1024)
#define COMMAND_TASK_PERIOD_MS (20)

static TaskHandle_t commandTaskHandle = NULL;
static QueueHandle_t commandQueue = NULL;

void command_task_init(void)
{
    int ret; // return value checker
    // initalize queue
    ret = xQueueCreateStatic();
    if (ret == 0) {
        printf("[COMMAND_TASK] Failred to create Static Queue.\n");
        return;
    }

    xTaskCreateStatic(
        command_task,
        "Command",
        COMMAND_TASK_STACK_SIZE,
        NULL,
        COMMAND_TASK_PRIORITY,
        &commandTaskHandle
    );
}

void command_task(void *pvParameters)
{
    (void) pvParameters;

    TickType_t lastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        // do some stuff

        xTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(COMMAND_TASK_PERIOD_MS)
        );
    }
}