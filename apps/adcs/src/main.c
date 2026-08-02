#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "../../../platform/sim/include/v_bus.h"

static void vControlTask(void *pvParameters)
{
    (void)pvParameters;
    while(1)
    {
        printf("[CONTROL] Running control loop/\n");

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void vCommandTask(void *pvParameters)
{
    (void)pvParameters;
    while(1)
    {
        printf("[COMMAND] Checking for commands\n");

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void vTelemetryTask(void *pvParameters)
{
    (void)pvParameters;

    while(1)
    {
        printf("[TELEM] Sending telemetry\n");

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main(void)
{
    printf("\n");
    printf("_____________\n");
    printf("ADCS POSIX FREERTOS TEST");
    printf("_____________\n");

    printf("Setting up v_bus.");
    
    VBus_t v_bus = create_v_bus();
    v_bus.initialize(1); // make master

    if (xTaskCreate(
            vControlTask,
            "Control",
            configMINIMAL_STACK_SIZE,
            NULL,
            3,
            NULL
        ) != pdPASS)
    {
        printf("ERROR: Failed to create Control task\n");
        return 1;
    }

    if (xTaskCreate(
            vCommandTask,
            "Command",
            configMINIMAL_STACK_SIZE,
            NULL,
            1,
            NULL
        ) != pdPASS)
    {
        printf("ERROR: Failed to create Command task\n");
        return 1;
    }

    if (xTaskCreate(
            vTelemetryTask,
            "Telemetry",
            configMINIMAL_STACK_SIZE,
            NULL,
            1,
            NULL
        ) != pdPASS)
    {
        printf("ERROR: Failed to create Telemetry task\n");
        return 1;
    }

    printf("All tasks created successfully.\n");
    printf("Starting FreeRTOS scheduler...\n\n");

    vTaskStartScheduler();

    printf("ERROR: Scheduler stopped!\n");

    return 1;
}

