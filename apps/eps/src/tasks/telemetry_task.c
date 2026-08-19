/*
Telemetry Task
1-10HZ
Collects everything
Sends out data using CSP
*/

#include "../../include/tasks/telemetry_task.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>
#include <string.h>

#include <csp/csp.h>

#include "csp_commands.h"

#define TELEMETRY_TASK_PRIORITY (1)
#define TELEMETRY_TASK_STACK_SIZE (1024)
#define TELEMETRY_TASK_PERIOD_MS (1000)

static StackType_t xTelemetryTaskStack[TELEMETRY_TASK_STACK_SIZE];
static StaticTask_t xTelemetryTaskBuffer;

TaskHandle_t xTelemetryHandle = NULL;

// reports the current position back to the OBC over CSP
static void telemetry_send_position(int32_t current_position)
{
    csp_conn_t * conn = csp_connect(CSP_PRIO_NORM, OBC_ADDRESS, ADCS_TELEM_PORT, 1000, CSP_O_NONE);
    if (conn == NULL) {
        printf("[TELEMETRY] Failed to connect to OBC\n");
        fflush(stdout);
        return;
    }

    csp_packet_t * packet = csp_buffer_get(0);
    if (packet == NULL) {
        printf("[TELEMETRY] Failed to get CSP buffer\n");
        fflush(stdout);
        csp_close(conn);
        return;
    }

    position_telemetry_t telem = { .current_position = current_position };
    memcpy(packet->data, &telem, sizeof(telem));
    packet->length = sizeof(telem);

    csp_send(conn, packet);
    csp_close(conn);
}

void telemetry_task_init(void)
{
    xTelemetryHandle = xTaskCreateStatic(
        telemetry_task,
        "telemetry",
        TELEMETRY_TASK_STACK_SIZE,
        NULL,
        TELEMETRY_TASK_PRIORITY,
        xTelemetryTaskStack,
        &xTelemetryTaskBuffer
    );

    if (xTelemetryHandle == NULL) {
        printf("[TELEMTRY] Failed to initialize.\n");
    }
}

void telemetry_task(void *pvParameters)
{
    (void) pvParameters;

    TickType_t lastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        // do some stuff

        uint32_t notified_value;
        if (xTaskNotifyWait(0, 0, &notified_value, 0) == pdTRUE)
        {
            int32_t target_position = (int32_t)notified_value;
            printf("[TELEMETRY] Reporting new position to OBC: %d\n", target_position);
            fflush(stdout);
            telemetry_send_position(target_position);
        }

        xTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(TELEMETRY_TASK_PERIOD_MS)
        );
    }
}