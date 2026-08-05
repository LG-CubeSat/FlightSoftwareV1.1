#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "../../../platform/sim/include/v_bus.h"

#define RECV_BUFFER_SIZE 128

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
    VBus_t *p_v_bus = (VBus_t *)pvParameters;

    uint8_t rx_buffer[RECV_BUFFER_SIZE];

    while(1)
    {
        int bytes_received = p_v_bus->receive(rx_buffer, RECV_BUFFER_SIZE);
        if (bytes_received > 0)
        {
            printf("[COMMAND] Received %d bytes: %.s\n", bytes_received, rx_buffer);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void vTelemetryTask(void *pvParameters)
{
    VBus_t *p_v_bus = (VBus_t *)pvParameters;

    while(1)
    {
        const char message[] = "Hello World from ADCS";
        const uint8_t *tx_data = (uint8_t *)message;

        size_t length = strlen(message);

        int sent_bytes = p_v_bus->send(tx_data, (uint16_t)length);

        if (sent_bytes == (int)length)
        {
            printf("[TELEM] Send %d bytes to OBC\n", sent_bytes);
        }


        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main(void)
{
    printf("\n");
    printf("_____________\n");
    printf("ADCS POSIX FREERTOS TEST\n");
    printf("_____________\n");
    fflush(stdout);

    printf("Setting up v_bus.\n");
    fflush(stdout);

    VBus_t v_bus = create_v_bus();
    if (v_bus.initialize(0) != V_BUS_OK) {
        fprintf(stderr, "ADCS: v_bus initialize failed\n");
        return 1;
    }

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
            (void *)&v_bus,
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
            (void *)&v_bus,
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

