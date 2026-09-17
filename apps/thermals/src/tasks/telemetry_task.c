#include "../../include/tasks/telemetry_task.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>
#include <string.h>

#include <csp/csp.h>

#include "csp_commands.h"
#include "thermal_data.h"

_Static_assert(
    MAX_SENSORS == THERMALS_TELEMETRY_SENSOR_COUNT,
    "Thermals sensor count must match the CSP telemetry packet"
);
//generates error if the csp_commands and thermal_data MAX_SENSORS values are different


#define TELEMETRY_TASK_PRIORITY (1)
#define TELEMETRY_TASK_STACK_SIZE (1024)

static StackType_t xTelemetryTaskStack[TELEMETRY_TASK_STACK_SIZE];
static StaticTask_t xTelemetryTaskBuffer;

TaskHandle_t xTelemetryHandle = NULL;

//sends current thermal values over CSP to the OBC
static void telemetry_send_thermal_values(ThermalData_t thermalData)
{
    csp_conn_t * conn = csp_connect(CSP_PRIO_NORM, OBC_ADDRESS, THERMALS_TELEM_PORT, 1000, CSP_O_NONE);
    if (conn == NULL) {
        printf("[THERMALS] Failed to connect to OBC\n");
        fflush(stdout);
        return;
    }

    csp_packet_t * packet = csp_buffer_get(0);
    if (packet == NULL) {
        printf("[THERMALS] Failed to get CSP buffer\n");
        fflush(stdout);
        csp_close(conn);
        return;
    }

    thermals_telemetry_t telem = {
    .valid_sensor_mask = thermalData.valid_sensor_mask,
    .average_temp = thermalData.average_temp,
    .target_temp = thermalData.target_temp
};

memcpy(
    telem.current_temps,
    thermalData.temperatures,
    sizeof(telem.current_temps)
);
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
    } else {
        printf("[TELEMTRY] Task created successfully.\n");
    }
}

void telemetry_task(void *pvParameters)
{
    (void) pvParameters;

    for (;;)
    {
        uint32_t notified_value;
        ThermalData_t thermalData;

        if (xTaskNotifyWait(
        0,
        UINT32_MAX,
        &notified_value,
        portMAX_DELAY
    ) == pdTRUE)
        {
            ThermalData_t thermalData = get_thermal_data();
            for (uint8_t i = 0; i < MAX_SENSORS; i++)
                {
                    printf(
                        "[TELEMETRY] Reporting Sensor %u temperature: %.2f C\n",
                        (unsigned int)(i + 1),
                        thermalData.temperatures[i]
                    );
                }
                printf(
                    "[TELEMETRY] Reporting average temperature: %.2f C\n",
                    thermalData.average_temp
                );

                printf(

                    "[TELEMETRY] Reporting target temperature: %.2f C\n",
                    thermalData.target_temp
                );
            //this should return the value of thermalData (impliment once we have hardware)
            printf("[TELEMETRY] Thermals functions to change to temperature initialized\n");
            fflush(stdout);
            telemetry_send_thermal_values(thermalData);
        }
    }
}
