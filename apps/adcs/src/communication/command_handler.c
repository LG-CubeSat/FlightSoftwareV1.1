#include "communication/command_handler.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include <csp/csp.h>

#include "board_shutdown.h"
#include "manager/fault_manager.h"
#include "tasks/command_task.h"

static command_ack_status_t enqueue_command(const adcs_command_t *command) {
    return command_task_send(command) != 0 ? ACK : NACK;
}

static command_ack_status_t decode_command(
    const csp_packet_t *packet,
    const command_envelope_t *envelope,
    uint8_t *reset_requested,
    uint8_t *shutdown_requested) {
    adcs_command_t command;

    memset(&command, 0, sizeof(command));
    command.sequence = envelope->seq;

    switch (envelope->command_id) {
        case CMD_MOVE_TO_POSITION:
            if (packet->length >= sizeof(position_command_t)) {
                position_command_t payload;
                memcpy(&payload, packet->data, sizeof(payload));
                if (fault_management_check_bounds(payload.target_position)) {
                    return NACK;
                }
                command.type = ADCS_COMMAND_LEGACY_POSITION;
                command.parameter.legacy_position = payload.target_position;
                return enqueue_command(&command);
            }
            return NACK;
        case CMD_RESET:
            *reset_requested = 1U;
            return ACK;
        case CMD_SHUTDOWN:
            *shutdown_requested = 1U;
            return ACK;
        case CMD_POINT_TO_SUN:
            command.type = ADCS_COMMAND_SET_MODE;
            command.parameter.mode = ADCS_MODE_SUN_ACQUISITION;
            printf("[COMMAND HANDLER] Point to sun command received. \n");
            fflush(stdout);
            return enqueue_command(&command);
        case CMD_TIME_SYNC:
            if (packet->length >= sizeof(time_sync_command_t)) {
                time_sync_command_t payload;
                memcpy(&payload, packet->data, sizeof(payload));
                if (payload.unix_time_sec <= 0 ||
                    (uint64_t)payload.unix_time_sec >
                        UINT64_MAX / 1000000ULL) {
                    return NACK;
                }
                command.type = ADCS_COMMAND_SET_UNIX_TIME;
                command.parameter.unix_time_sec = payload.unix_time_sec;
                printf("[COMMAND HANDLER] Time sync command received. \n");
                fflush(stdout);
                return enqueue_command(&command);
            }
            return NACK;
        case ADCS_WIRE_COMMAND_SET_MODE:
            if (packet->length >= sizeof(adcs_mode_command_payload_t)) {
                adcs_mode_command_payload_t payload;
                memcpy(&payload, packet->data, sizeof(payload));
                if (payload.mode >= ADCS_MODE_COUNT || payload.mode == ADCS_MODE_BOOT) {
                    return NACK;
                }
                command.type = ADCS_COMMAND_SET_MODE;
                command.parameter.mode = (adcs_mode_t)payload.mode;
                return enqueue_command(&command);
            }
            return NACK;
        case ADCS_WIRE_COMMAND_SET_ATTITUDE:
            if (packet->length >= sizeof(adcs_attitude_command_payload_t)) {
                adcs_attitude_command_payload_t payload;
                memcpy(&payload, packet->data, sizeof(payload));
                command.type = ADCS_COMMAND_SET_ATTITUDE;
                memcpy(command.parameter.attitude, payload.target_quaternion, sizeof(versor));
                return enqueue_command(&command);
            }
            return NACK;
        case ADCS_WIRE_COMMAND_SET_POINTING_VECTOR:
            if (packet->length >= sizeof(adcs_pointing_command_payload_t)) {
                adcs_pointing_command_payload_t payload;
                memcpy(&payload, packet->data, sizeof(payload));
                command.type = ADCS_COMMAND_SET_POINTING_VECTOR;
                memcpy(
                    command.parameter.pointing.body_axis,
                    payload.body_axis,
                    sizeof(payload.body_axis));
                memcpy(
                    command.parameter.pointing.inertial_direction,
                    payload.inertial_direction,
                    sizeof(payload.inertial_direction));
                return enqueue_command(&command);
            }
            return NACK;
        case ADCS_WIRE_COMMAND_RESET_ESTIMATOR:
            command.type = ADCS_COMMAND_RESET_ESTIMATOR;
            return enqueue_command(&command);
        case ADCS_WIRE_COMMAND_SET_ACTUATOR_INHIBIT:
            if (packet->length >= sizeof(adcs_actuator_inhibit_payload_t)) {
                adcs_actuator_inhibit_payload_t payload;
                memcpy(&payload, packet->data, sizeof(payload));
                command.type = ADCS_COMMAND_DISABLE_ACTUATORS;
                command.parameter.actuators_inhibited = payload.inhibited != 0U;
                return enqueue_command(&command);
            }
            return NACK;
        case ADCS_WIRE_COMMAND_SIMULATOR_FAULT:
            if (packet->length >= sizeof(adcs_simulator_fault_payload_t)) {
                adcs_simulator_fault_payload_t payload;
                memcpy(&payload, packet->data, sizeof(payload));
                command.type = ADCS_COMMAND_SIMULATOR_FAULT;
                command.parameter.simulator_fault_mask = payload.fault_mask;
                return enqueue_command(&command);
            }
            return NACK;
        default:
            return NACK;
    }
}

static void send_ack(csp_conn_t *connection, const command_ack_t *acknowledgement) {
    csp_packet_t *reply = csp_buffer_get(0);

    if (reply == NULL) {
        fprintf(stderr, "[COMMAND HANDLER] Failed to get packet buffer for ACK.\n");
        return;
    }
    memcpy(reply->data, acknowledgement, sizeof(*acknowledgement));
    reply->length = sizeof(*acknowledgement);
    csp_send(connection, reply);
}

static void *command_handler_rx_loop(void *parameter) {
    csp_socket_t socket = {0};

    (void)parameter;
    if (csp_bind(&socket, ADCS_CMD_PORT) != CSP_ERR_NONE) {
        fprintf(stderr, "[COMMAND HANDLER] csp_bind failed\n");
        return NULL;
    }
    csp_listen(&socket, 5);

    for (;;) {
        csp_conn_t *connection = csp_accept(&socket, 10000);
        csp_packet_t *packet;

        if (connection == NULL) {
            continue;
        }
        while ((packet = csp_read(connection, 50)) != NULL) {
            if (csp_conn_dport(connection) == ADCS_CMD_PORT &&
                packet->length >= sizeof(command_envelope_t)) {
                command_envelope_t envelope;
                command_ack_t acknowledgement;
                uint8_t reset_requested = 0U;
                uint8_t shutdown_requested = 0U;

                memcpy(&envelope, packet->data, sizeof(envelope));
                acknowledgement.ack_command_id = envelope.command_id;
                acknowledgement.ack_seq = envelope.seq;
                acknowledgement.status = decode_command(
                    packet,
                    &envelope,
                    &reset_requested,
                    &shutdown_requested);
                send_ack(connection, &acknowledgement);
                csp_buffer_free(packet);

                if (reset_requested != 0U) {
                    printf("[COMMAND HANDLER] Reset command received -- resetting now\n");
                    fflush(stdout);
                    fault_management_trigger_reset(RESET_REASON_WATCHDOG);
                }
                if (shutdown_requested != 0U) {
                    printf("[COMMAND HANDLER] Shutdown command received. Shutting down.\n");
                    fflush(stdout);
                    board_shutdown();
                }
                continue;
            }
            csp_buffer_free(packet);
        }
        csp_close(connection);
    }
    return NULL;
}

void command_handler_init(void) {
    pthread_t receive_thread;

    if (pthread_create(
            &receive_thread,
            NULL,
            command_handler_rx_loop,
            NULL) != 0) {
        fprintf(stderr, "[COMMAND HANDLER] receive thread create failed\n");
        return;
    }
    pthread_detach(receive_thread);
}
