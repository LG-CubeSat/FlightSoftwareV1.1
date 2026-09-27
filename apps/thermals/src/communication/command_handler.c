#include "communication/command_handler.h"

#include <csp/csp.h>

#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "csp_commands.h"
#include "tasks/command_task.h"

#include "thermal_data.h"

static command_ack_status_t enqueue_command(
    const thermals_command_t *command)
{
    return command_task_send(command) != 0 ? ACK : NACK;
}

static command_ack_status_t decode_command(
    const csp_packet_t *packet,
    const command_envelope_t *envelope)
{
    thermals_command_t command;

    memset(&command, 0, sizeof(command));
    command.sequence = envelope->seq;

    switch (envelope->command_id)
    {
        case THERMALS_WIRE_COMMAND_SET_TARGET_TEMP:
        {
            thermal_command_t payload;

            if (packet->length < sizeof(payload))
            {
                return NACK;
            }

            memcpy(&payload, packet->data, sizeof(payload));

            if (!thermals_target_is_valid(payload.target_temp))
            {
                return NACK;
            }

            command.type = THERMALS_COMMAND_SET_TARGET_TEMP;
            command.parameter.target_temp = payload.target_temp;

            return enqueue_command(&command);
        }

        case THERMALS_WIRE_COMMAND_REQUEST_TELEMETRY:
            command.type = THERMALS_COMMAND_REQUEST_TELEMETRY;
            return enqueue_command(&command);

        default:
            return NACK;
    }
}

static void send_ack(
    csp_conn_t *connection,
    const command_ack_t *acknowledgement)
{
    csp_packet_t *reply = csp_buffer_get(0);

    if (reply == NULL)
    {
        printf(
            "[THERMALS COMMAND HANDLER] Failed to allocate ACK packet.\n"
        );
        fflush(stdout);
        return;
    }

    memcpy(
        reply->data,
        acknowledgement,
        sizeof(*acknowledgement)
    );

    reply->length = sizeof(*acknowledgement);
    csp_send(connection, reply);
}

static void *command_handler_rx_loop(void *param)
{
    csp_socket_t socket = {0};

    (void)param;

    if (csp_bind(&socket, THERMALS_CMD_PORT) != CSP_ERR_NONE)
    {
        printf("[THERMALS COMMAND HANDLER] csp_bind failed\n");
        fflush(stdout);
        return NULL;
    }

    csp_listen(&socket, 5);

    for (;;)
    {
        csp_conn_t *connection =
            csp_accept(&socket, 10000);

        if (connection == NULL)
        {
            continue;
        }

        csp_packet_t *packet;

        while ((packet = csp_read(connection, 50)) != NULL)
        {
            if (csp_conn_dport(connection) == THERMALS_CMD_PORT &&
                packet->length >= sizeof(command_envelope_t))
            {
                command_envelope_t envelope;
                command_ack_t acknowledgement;

                memcpy(
                    &envelope,
                    packet->data,
                    sizeof(envelope)
                );

                acknowledgement.ack_command_id =
                    envelope.command_id;

                acknowledgement.ack_seq =
                    envelope.seq;

                acknowledgement.status =
                    decode_command(packet, &envelope);

                send_ack(connection, &acknowledgement);

                printf(
                    "[THERMALS COMMAND HANDLER] Command=%u sequence=%lu status=%s\n",
                    (unsigned int)envelope.command_id,
                    (unsigned long)envelope.seq,
                    acknowledgement.status == ACK ? "ACK" : "NACK"
                );

                fflush(stdout);
            }
            else
            {
                printf("[THERMALS COMMAND HANDLER] Invalid command packet.\n");
                fflush(stdout);
            }

            csp_buffer_free(packet);
        }

        csp_close(connection);
    }

    return NULL;
}

void command_handler_init(void)
{
    pthread_t rx_thread;
    int ret = pthread_create(&rx_thread, NULL, command_handler_rx_loop, NULL);

    if (ret != 0)
    {
        printf("[THERMALS COMMAND HANDLER] rx thread create failed: %d\n", ret);
        fflush(stdout);
    }
    else { pthread_detach(rx_thread); }
}
