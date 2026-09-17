#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include <math.h>


#include "thermal_data.h"
#include "tasks/heater_set_task.h"
#include "tasks/sensor_read_task.h"
#include "tasks/telemetry_task.h"

#include <stdio.h>
#include "../../include/tasks/command_task.h"

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
    } else {
        printf("[THERMAL_COMMAND] Task created successfully.\n");
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

            if (message.command == THERMAL_CMD_SET_TARGET_TEMP) { //@param parameter Recieved via obc, sets goal temperature to @param paramater

                if (!isfinite(message.parameter))
                {
                    printf("[THERMALS COMMAND] Invalid target temperature received\n");
                    fflush(stdout);
                    continue;
                }

                thermals_set_target(message.parameter);

                printf("[THERMALS COMMAND] Setting target temperature to %f C\n", message.parameter);
                fflush(stdout);
                
            } else if (message.command == THERMAL_CMD_REQUEST_TELEMETRY) /*Obc requests current temp and goal temp, send via telem*/{

                if (xTelemetryHandle == NULL)
                {
                    printf("[THERMALS COMMAND] Telemetry task is unavailable\n");
                    fflush(stdout);
                }
                else if (xTaskNotify(xTelemetryHandle, 0, eNoAction) != pdPASS)
                {
                    printf("[THERMALS COMMAND] Failed to notify telemetry task\n");
                    fflush(stdout);
                }

             } else {
                printf("[THERMALS COMMAND] Unknown command received: %lu\n",(unsigned long)message.command);
                fflush(stdout);

             }
    }
}
}
