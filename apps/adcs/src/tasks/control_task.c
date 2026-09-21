#include "tasks/control_task.h"

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "control/actuator_allocator.h"
#include "control/controller.h"
#include "magnetorquer.h"
#include "manager/adcs_manager.h"
#include "manager/fault_manager.h"
#include "simulation/adcs_simulator.h"

#define CONTROL_TASK_PRIORITY 4
#define CONTROL_TASK_STACK_SIZE 2048
#define CONTROL_TASK_PERIOD_MS 50

static StackType_t control_task_stack[CONTROL_TASK_STACK_SIZE];
static StaticTask_t control_task_buffer;
static adcs_controller_t controller;

TaskHandle_t xControlHandle;

static adcs_controller_config_t controller_config(void) {
    return (adcs_controller_config_t) {
        .bdot = {
            .gain_a_m2_s_t = 30000.0F,
            .derivative_filter_alpha = 0.25F,
            .maximum_dipole_a_m2 = 0.20F,
            .minimum_dt_s = 0.01F,
            .maximum_dt_s = 0.20F
        },
        .slew = {
            .attitude_rate_gain_s = 0.02F,
            .rate_pid = {
                .proportional_gain = 0.00100F,
                .integral_gain = 0.0F,
                .derivative_gain = 0.0F,
                .integrator_limit = 0.20F,
                .output_limit = 0.000008F
            },
            .maximum_torque_nm = 0.000008F,
            .maximum_rate_rad_s = 0.08F,
            .settled_angle_rad = 0.05235988F,
            .settled_rate_rad_s = 0.008F,
            .settled_cycles_required = 20U
        },
        .sun_pointing = {
            .proportional_gain_nm = 0.00002F,
            .rate_damping_gain_nm_s = 0.00010F,
            .maximum_torque_nm = 0.00002F,
            .minimum_sun_irradiance_w_m2 = 100.0F,
            .pointing_axis_body = {1.0F, 0.0F, 0.0F}
        },
        .allocator = {
            .maximum_axis_dipole_a_m2 = {0.20F, 0.20F, 0.20F},
            .minimum_usable_field_t = 1.0e-6F
        }
    };
}

void control_task_init(void) {
    adcs_controller_config_t config = controller_config();

    adcs_controller_init(&controller, &config);
    xControlHandle = xTaskCreateStatic(
        control_task,
        "Control",
        CONTROL_TASK_STACK_SIZE,
        NULL,
        CONTROL_TASK_PRIORITY,
        control_task_stack,
        &control_task_buffer);
    if (xControlHandle == NULL) {
        printf("[CONTROL] Task creation failed.\n");
    }
}

void control_task(void *parameters) {
    TickType_t last_wake_time = xTaskGetTickCount();

    (void)parameters;
    for (;;) {
        adcs_manager_state_t snapshot;
        adcs_control_output_t output;
        magnetorquer_command_t command;
        adcs_result_t result;
        uint32_t notification;

        adcs_manager_update(adcs_simulator_get_time_us());
        adcs_manager_get_state(&snapshot);
        result = adcs_controller_update(
            &controller,
            snapshot.mode,
            &snapshot.guidance_target,
            &snapshot.latest_sensors,
            &snapshot.latest_attitude,
            (float)CONTROL_TASK_PERIOD_MS / 1000.0F,
            &output);

        memset(&command, 0, sizeof(command));
        if (result == ADCS_RESULT_OK &&
            snapshot.actuators_inhibited == 0U &&
            output.actuators_enabled != 0U) {
            memcpy(
                command.dipole_a_m2,
                output.requested_dipole_a_m2,
                sizeof(command.dipole_a_m2));
            command.enabled = 1U;
        } else {
            output.actuators_enabled = 0U;
            memset(output.requested_dipole_a_m2, 0, sizeof(output.requested_dipole_a_m2));
            if (result != ADCS_RESULT_OK &&
                snapshot.mode != ADCS_MODE_BOOT && snapshot.mode != ADCS_MODE_SAFE) {
                adcs_manager_note_controller_error();
            }
        }

        if (magnetorquer_set(&command) != MAGNETORQUER_OK) {
            output.actuators_enabled = 0U;
            fault_management_report(ADCS_FAULT_ACTUATOR);
        }
        adcs_manager_set_control_output(&output);

        if (xTaskNotifyWait(0U, UINT32_MAX, &notification, 0U) == pdTRUE) {
            printf("[CONTROL] Slewing toward target position: %d\n",
                   (int32_t)notification);
            fflush(stdout);
        }
        xTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(CONTROL_TASK_PERIOD_MS));
    }
}
