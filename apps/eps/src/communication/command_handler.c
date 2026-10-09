#include "communication/command_handler.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include <csp/csp.h>

#include "board_shutdown.h"
#include "manager/fault_manager.h"
#include "power_rail.h"
#include "tasks/command_task.h"

static command_ack_status_t enqueue_command(const eps_command_t *command) {
    return command_task_send(command) != 0 ? ACK : NACK;
}

typedef enum {
    COMMAND_ACTION_NONE = 0,
    COMMAND_ACTION_RESET,
    COMMAND_ACTION_SHUTDOWN
} command_action_t;

static command_ack_status_t decode_command(
    const csp_packet_t *packet,
    const command_envelope_t *envelope,
    command_action_t *action) {
    eps_command_t command;

    memset(&command, 0, sizeof(command));
    command.sequence = envelope->seq;

    switch (envelope->command_id) {
        case CMD_RESET:
            *action = COMMAND_ACTION_RESET;
            return ACK;
        case CMD_SHUTDOWN:
            *action = COMMAND_ACTION_SHUTDOWN;
            return ACK;
        case CMD_SET_SAFE_MODE:
            command.type = EPS_COMMAND_SET_MODE;
            command.parameter.mode = EPS_MODE_SAFE;
            return enqueue_command(&command);
        case CMD_TIME_SYNC:
            if (packet->length >= sizeof(time_sync_command_t)) {
                time_sync_command_t payload;
                memcpy(&payload, packet->data, sizeof(payload));
                if (payload.unix_time_sec <= 0 ||
                    (uint64_t)payload.unix_time_sec > UINT64_MAX / 1000000ULL) {
                    return NACK;
                }
                command.type = EPS_COMMAND_SET_UNIX_TIME;
                command.parameter.unix_time_sec = payload.unix_time_sec;
                return enqueue_command(&command);
            }
            return NACK;
        case EPS_WIRE_COMMAND_SET_MODE:
            if (packet->length >= sizeof(eps_mode_command_payload_t)) {
                eps_mode_command_payload_t payload;
                memcpy(&payload, packet->data, sizeof(payload));
                if (payload.mode >= EPS_MODE_COUNT || payload.mode == EPS_MODE_BOOT) {
                    return NACK;
                }
                command.type = EPS_COMMAND_SET_MODE;
                command.parameter.mode = (eps_mode_t)payload.mode;
                return enqueue_command(&command);
            }
            return NACK;
        case EPS_WIRE_COMMAND_SET_RAIL:
            if (packet->length >= sizeof(eps_rail_command_payload_t)) {
                eps_rail_command_payload_t payload;
                memcpy(&payload, packet->data, sizeof(payload));
                if (payload.rail_id >= POWER_RAIL_COUNT) {
                    return NACK;
                }
                command.type = EPS_COMMAND_SET_RAIL;
                command.parameter.rail.rail_id = payload.rail_id;
                command.parameter.rail.enabled = payload.enabled != 0U ? 1U : 0U;
                return enqueue_command(&command);
            }
            return NACK;
        case EPS_WIRE_COMMAND_POWER_CYCLE:
            if (packet->length >= sizeof(eps_power_cycle_command_payload_t)) {
                eps_power_cycle_command_payload_t payload;
                memcpy(&payload, packet->data, sizeof(payload));
                if (payload.rail_id >= POWER_RAIL_COUNT) {
                    return NACK;
                }
                command.type = EPS_COMMAND_POWER_CYCLE;
                command.parameter.power_cycle_rail_id = payload.rail_id;
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
    if (csp_bind(&socket, EPS_CMD_PORT) != CSP_ERR_NONE) {
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
            if (csp_conn_dport(connection) == EPS_CMD_PORT &&
                packet->length >= sizeof(command_envelope_t)) {
                command_envelope_t envelope;
                command_ack_t acknowledgement;
                command_action_t action = COMMAND_ACTION_NONE;

                memcpy(&envelope, packet->data, sizeof(envelope));
                acknowledgement.ack_command_id = envelope.command_id;
                acknowledgement.ack_seq = envelope.seq;
                acknowledgement.status = decode_command(
                    packet,
                    &envelope,
                    &action);
                send_ack(connection, &acknowledgement);
                csp_buffer_free(packet);

                if (acknowledgement.status == ACK &&
                    envelope.command_id == CMD_TIME_SYNC) {
                    printf("[COMMAND HANDLER] Time sync command received.\n");
                    fflush(stdout);
                }

                switch (action) {
                    case COMMAND_ACTION_RESET:
                        printf("[COMMAND HANDLER] Reset command received -- resetting now\n");
                        fflush(stdout);
                        fault_management_trigger_reset(RESET_REASON_WATCHDOG);
                        break;
                    case COMMAND_ACTION_SHUTDOWN:
                        printf("[COMMAND HANDLER] Shutdown command received. Shutting down.\n");
                        fflush(stdout);
                        board_shutdown();
                        break;
                    case COMMAND_ACTION_NONE:
                    default:
                        break;
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
