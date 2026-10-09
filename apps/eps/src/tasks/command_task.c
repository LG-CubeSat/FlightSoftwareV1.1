#include "tasks/command_task.h"

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "manager/eps_manager.h"
#include "power_rail.h"
#include "simulation/eps_simulator.h"

#define COMMAND_TASK_PRIORITY 2
#define COMMAND_TASK_STACK_SIZE 1536
#define COMMAND_QUEUE_LENGTH 8

static StackType_t command_task_stack[COMMAND_TASK_STACK_SIZE];
static StaticTask_t command_task_buffer;
static StaticQueue_t command_queue_buffer;
static uint8_t command_queue_storage[COMMAND_QUEUE_LENGTH * sizeof(eps_command_t)];
TaskHandle_t xCommandHandle;
static QueueHandle_t command_queue;

int command_task_send(const eps_command_t *message) {
    if (command_queue == NULL || message == NULL) {
        return 0;
    }
    /* The command handler runs on a plain pthread outside the FreeRTOS
       scheduler, so use the ISR-safe send that never tries to yield -- same
       pattern the ADCS command task uses. */
    if (xQueueSendFromISR(command_queue, message, NULL) != pdPASS) {
        eps_manager_note_dropped_message();
        return 0;
    }
    return 1;
}

void command_task_init(void) {
    command_queue = xQueueCreateStatic(
        COMMAND_QUEUE_LENGTH,
        sizeof(eps_command_t),
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

static void execute_command(const eps_command_t *command) {
    switch (command->type) {
        case EPS_COMMAND_SET_MODE:
            printf("[COMMAND] Setting EPS mode to %s\n",
                   eps_manager_mode_name(command->parameter.mode));
            fflush(stdout);
            eps_manager_request_mode(command->parameter.mode);
            break;
        case EPS_COMMAND_SET_RAIL:
            printf("[COMMAND] Rail %d -> %s\n",
                   command->parameter.rail.rail_id,
                   command->parameter.rail.enabled != 0U ? "on" : "off");
            fflush(stdout);
            if (power_rail_set_enabled(
                    (power_rail_id_t)command->parameter.rail.rail_id,
                    command->parameter.rail.enabled) != POWER_RAIL_OK) {
                eps_manager_note_rejected_command();
            } else {
                eps_manager_note_rail_switch();
            }
            break;
        case EPS_COMMAND_POWER_CYCLE:
            printf("[COMMAND] Power-cycling rail %d\n",
                   command->parameter.power_cycle_rail_id);
            fflush(stdout);
            if (power_rail_set_enabled(
                    (power_rail_id_t)command->parameter.power_cycle_rail_id,
                    0U) != POWER_RAIL_OK ||
                power_rail_set_enabled(
                    (power_rail_id_t)command->parameter.power_cycle_rail_id,
                    1U) != POWER_RAIL_OK) {
                eps_manager_note_rejected_command();
            } else {
                eps_manager_note_rail_switch();
            }
            break;
        case EPS_COMMAND_SET_UNIX_TIME:
            if (command->parameter.unix_time_sec > 0 &&
                (uint64_t)command->parameter.unix_time_sec <=
                    UINT64_MAX / 1000000ULL) {
                if (eps_simulator_set_unix_time(
                        (uint64_t)command->parameter.unix_time_sec * 1000000ULL) ==
                    EPS_RESULT_OK) {
                    printf("[COMMAND] Time sync command received.\n");
                    fflush(stdout);
                } else {
                    eps_manager_note_rejected_command();
                }
            } else {
                eps_manager_note_rejected_command();
            }
            break;
        case EPS_COMMAND_NONE:
        default:
            eps_manager_note_rejected_command();
            break;
    }
}

void command_task(void *parameters) {
    eps_command_t command;

    (void)parameters;
    for (;;) {
        if (xQueueReceive(command_queue, &command, portMAX_DELAY) == pdPASS) {
            execute_command(&command);
        }
    }
}
