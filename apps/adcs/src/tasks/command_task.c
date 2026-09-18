#include "tasks/command_task.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "guidance/target_generator.h"
#include "manager/adcs_manager.h"
#include "simulation/adcs_simulator.h"
#include "tasks/control_task.h"
#include "tasks/estimation_task.h"
#include "tasks/sensor_task.h"
#include "tasks/telemetry_task.h"

#define COMMAND_TASK_PRIORITY 2
#define COMMAND_TASK_STACK_SIZE 1536
#define COMMAND_QUEUE_LENGTH 8
#define COMMAND_DEFAULT_MAXIMUM_RATE_RAD_S 0.08F
#define COMMAND_PI_F 3.14159265358979323846F

static StackType_t command_task_stack[COMMAND_TASK_STACK_SIZE];
static StaticTask_t command_task_buffer;
static StaticQueue_t command_queue_buffer;
static uint8_t command_queue_storage[COMMAND_QUEUE_LENGTH * sizeof(adcs_command_t)];
TaskHandle_t xCommandHandle;
static QueueHandle_t command_queue;

int command_task_send(const adcs_command_t *message) {
    if (command_queue == NULL || message == NULL) {
        return 0;
    }
    if (xQueueSendFromISR(command_queue, message, NULL) != pdPASS) {
        adcs_manager_note_dropped_message();
        return 0;
    }
    return 1;
}

void command_task_init(void) {
    command_queue = xQueueCreateStatic(
        COMMAND_QUEUE_LENGTH,
        sizeof(adcs_command_t),
        command_queue_storage,
        &command_queue_buffer);
    if (command_queue == NULL) {
        printf("[COMMAND] Queue creation failed.\n");
        return;
    }

    xCommandHandle = xTaskCreateStatic(
        command_task,
        "Command",
        COMMAND_TASK_STACK_SIZE,
        NULL,
        COMMAND_TASK_PRIORITY,
        command_task_stack,
        &command_task_buffer);
    if (xCommandHandle == NULL) {
        printf("[COMMAND] Task creation failed.\n");
    }
}

static void dispatch_legacy_position(const adcs_command_t *command) {
    adcs_guidance_target_t target;
    float angle = fmodf((float)command->parameter.legacy_position, 360.0F) *
                  COMMAND_PI_F / 180.0F;
    versor quaternion = {
        0.0F,
        0.0F,
        -sinf(0.5F * angle),
        cosf(0.5F * angle)
    };
    uint32_t notification = (uint32_t)command->parameter.legacy_position;

    printf("[COMMAND] Dispatching position command: target=%d\n",
           command->parameter.legacy_position);
    fflush(stdout);
    if (adcs_guidance_target_from_attitude(
            ADCS_MODE_SLEWING,
            quaternion,
            COMMAND_DEFAULT_MAXIMUM_RATE_RAD_S,
            &target) != ADCS_RESULT_OK) {
        adcs_manager_note_rejected_command();
        return;
    }

    adcs_manager_set_guidance_target(&target);
    adcs_manager_set_legacy_position(command->parameter.legacy_position);
    adcs_manager_request_mode(ADCS_MODE_SLEWING);

    if (xControlHandle != NULL) {
        (void)xTaskNotify(xControlHandle, notification, eSetValueWithOverwrite);
    }
    if (xEstimationHandle != NULL) {
        (void)xTaskNotify(xEstimationHandle, notification, eSetValueWithOverwrite);
    }
    if (xSensorHandle != NULL) {
        (void)xTaskNotify(xSensorHandle, notification, eSetValueWithOverwrite);
    }
    if (xTelemetryHandle != NULL) {
        (void)xTaskNotify(xTelemetryHandle, notification, eSetValueWithOverwrite);
    }
}

static void execute_command(const adcs_command_t *command) {
    adcs_guidance_target_t target;

    switch (command->type) {
        case ADCS_COMMAND_SET_MODE:
            adcs_manager_request_mode(command->parameter.mode);
            break;
        case ADCS_COMMAND_SET_ATTITUDE:
            if (adcs_guidance_target_from_attitude(
                    ADCS_MODE_SLEWING,
                    command->parameter.attitude,
                    COMMAND_DEFAULT_MAXIMUM_RATE_RAD_S,
                    &target) == ADCS_RESULT_OK) {
                adcs_manager_set_guidance_target(&target);
                adcs_manager_request_mode(ADCS_MODE_SLEWING);
            } else {
                adcs_manager_note_rejected_command();
            }
            break;
        case ADCS_COMMAND_SET_POINTING_VECTOR:
            if (adcs_guidance_target_from_vector(
                    ADCS_MODE_SLEWING,
                    command->parameter.pointing.body_axis,
                    command->parameter.pointing.inertial_direction,
                    COMMAND_DEFAULT_MAXIMUM_RATE_RAD_S,
                    &target) == ADCS_RESULT_OK) {
                adcs_manager_set_guidance_target(&target);
                adcs_manager_request_mode(ADCS_MODE_SLEWING);
            } else {
                adcs_manager_note_rejected_command();
            }
            break;
        case ADCS_COMMAND_RESET_ESTIMATOR:
            estimation_task_request_reset();
            break;
        case ADCS_COMMAND_DISABLE_ACTUATORS:
            adcs_manager_set_actuators_inhibited(
                command->parameter.actuators_inhibited);
            break;
        case ADCS_COMMAND_SET_UNIX_TIME:
            if (command->parameter.unix_time_sec > 0) {
                uint64_t seconds =
                    (uint64_t)command->parameter.unix_time_sec;

                if (seconds <= UINT64_MAX / 1000000ULL) {
                    if (adcs_simulator_set_unix_time(seconds * 1000000ULL) ==
                        ADCS_RESULT_OK) {
                        printf("[COMMAND] Time sync command received.\n");
                        fflush(stdout);
                    } else {
                        adcs_manager_note_rejected_command();
                    }
                } else {
                    adcs_manager_note_rejected_command();
                }
            } else {
                adcs_manager_note_rejected_command();
            }
            break;
        case ADCS_COMMAND_SIMULATOR_FAULT:
            adcs_simulator_set_faults(command->parameter.simulator_fault_mask);
            break;
        case ADCS_COMMAND_LEGACY_POSITION:
            dispatch_legacy_position(command);
            break;
        case ADCS_COMMAND_NONE:
        default:
            adcs_manager_note_rejected_command();
            break;
    }
}

void command_task(void *parameters) {
    adcs_command_t command;

    (void)parameters;
    for (;;) {
        if (xQueueReceive(command_queue, &command, portMAX_DELAY) == pdPASS) {
            execute_command(&command);
        }
    }
}
