#include "tasks/telemetry_task.h"

#include <stdio.h>
#include <string.h>

#include <csp/csp.h>

#include "FreeRTOS.h"
#include "task.h"

#include "communication/telemetry.h"
#include "control/control_math.h"
#include "csp_commands.h"
#include "manager/adcs_manager.h"

#define TELEMETRY_TASK_PRIORITY 1
#define TELEMETRY_TASK_STACK_SIZE 1792
#define TELEMETRY_TASK_PERIOD_MS 200

static StackType_t telemetry_task_stack[TELEMETRY_TASK_STACK_SIZE];
static StaticTask_t telemetry_task_buffer;

TaskHandle_t xTelemetryHandle;

static void telemetry_send_legacy_position(int32_t current_position) {
    csp_conn_t *connection = csp_connect(
        CSP_PRIO_NORM,
        OBC_ADDRESS,
        ADCS_TELEM_PORT,
        100,
        CSP_O_NONE);
    csp_packet_t *packet;
    position_telemetry_t telemetry;

    if (connection == NULL) {
        return;
    }
    packet = csp_buffer_get(0);
    if (packet == NULL) {
        csp_close(connection);
        return;
    }
    telemetry.current_position = current_position;
    memcpy(packet->data, &telemetry, sizeof(telemetry));
    packet->length = sizeof(telemetry);
    csp_send(connection, packet);
    csp_close(connection);
}

void telemetry_task_init(void) {
    adcs_telemetry_init();
    xTelemetryHandle = xTaskCreateStatic(
        telemetry_task,
        "Telemetry",
        TELEMETRY_TASK_STACK_SIZE,
        NULL,
        TELEMETRY_TASK_PRIORITY,
        telemetry_task_stack,
        &telemetry_task_buffer);
    if (xTelemetryHandle == NULL) {
        printf("[TELEMETRY] Task creation failed.\n");
    }
}

void telemetry_task(void *parameters) {
    TickType_t last_wake_time = xTaskGetTickCount();
    uint32_t diagnostic_divider = 0U;

    (void)parameters;
    for (;;) {
        adcs_manager_state_t snapshot;
        adcs_telemetry_packet_t telemetry;
        uint32_t notification;

        adcs_manager_get_state(&snapshot);
        memset(&telemetry, 0, sizeof(telemetry));
        telemetry.timestamp_us = snapshot.latest_sensors.timestamp_us;
        telemetry.mode = snapshot.mode;
        telemetry.sensors = snapshot.latest_sensors;
        telemetry.attitude = snapshot.latest_attitude;
        telemetry.target = snapshot.guidance_target;
        telemetry.control = snapshot.latest_control;
        telemetry.health = snapshot.health;
        if (adcs_telemetry_send(&telemetry) != ADCS_TELEMETRY_OK) {
            adcs_manager_note_dropped_message();
        }

        if (++diagnostic_divider >= 5U) {
            diagnostic_divider = 0U;
            printf("[ADCS] mode=%s rate=%.4f q=[%.3f %.3f %.3f %.3f] "
                   "target=[%.3f %.3f %.3f %.3f] error=%.3f "
                   "wheel=[%.2e %.2e %.2e] dipole=[%.3f %.3f %.3f]\n",
                   adcs_manager_mode_name(snapshot.mode),
                   adcs_vector_norm(snapshot.latest_attitude.angular_rate_rad_s),
                   snapshot.latest_attitude.quaternion[0],
                   snapshot.latest_attitude.quaternion[1],
                   snapshot.latest_attitude.quaternion[2],
                   snapshot.latest_attitude.quaternion[3],
                   snapshot.guidance_target.target_quaternion[0],
                   snapshot.guidance_target.target_quaternion[1],
                   snapshot.guidance_target.target_quaternion[2],
                   snapshot.guidance_target.target_quaternion[3],
                   snapshot.latest_control.pointing_error_rad,
                   snapshot.latest_control.reaction_wheel_torque_nm[0],
                   snapshot.latest_control.reaction_wheel_torque_nm[1],
                   snapshot.latest_control.reaction_wheel_torque_nm[2],
                   snapshot.latest_control.requested_dipole_a_m2[0],
                   snapshot.latest_control.requested_dipole_a_m2[1],
                   snapshot.latest_control.requested_dipole_a_m2[2]);
            fflush(stdout);
        }

        if (xTaskNotifyWait(0U, UINT32_MAX, &notification, 0U) == pdTRUE) {
            printf("[TELEMETRY] Reporting new position to OBC: %d\n",
                   (int32_t)notification);
            fflush(stdout);
            telemetry_send_legacy_position((int32_t)notification);
        }
        xTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(TELEMETRY_TASK_PERIOD_MS));
    }
}
