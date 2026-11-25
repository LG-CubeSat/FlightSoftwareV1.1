#include "communication/command_handler.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include <csp/csp.h>
#include "csp_commands.h"

#include "tasks/telemetry_task.h"

// Keep the socket alive for the lifetime of the program.
static csp_socket_t command_socket = {0};

static void send_command_ack(
    csp_conn_t *connection,
    const command_ack_t *ack)
{
    csp_packet_t *reply = csp_buffer_get(0);

    if (reply == NULL) {
        fprintf(stderr, "[EPS COMMAND] Failed to allocate ACK packet.\n");
        return;
    }

    memcpy(reply->data, ack, sizeof(*ack));
    reply->length = sizeof(*ack);

    csp_send(connection, reply);
}

static void *command_receiver(void *argument)
{
    (void)argument;

    if (csp_bind(&command_socket, EPS_CMD_PORT) != CSP_ERR_NONE) {
        fprintf(stderr, "[EPS COMMAND] Failed to bind command port.\n");
        return NULL;
    }

    if (csp_listen(&command_socket, 5) != CSP_ERR_NONE) {
        fprintf(stderr, "[EPS COMMAND] Failed to listen.\n");
        return NULL;
    }

    printf("[EPS COMMAND] Listening on port %u.\n",
           (unsigned)EPS_CMD_PORT);
    fflush(stdout);

    for (;;) {
        csp_conn_t *connection =
            csp_accept(&command_socket, 1000);

        if (connection == NULL) {
            continue;
        }

        csp_packet_t *packet;

        while ((packet = csp_read(connection, 100)) != NULL) {
            if (csp_conn_src(connection) != OBC_ADDRESS) {
                fprintf(stderr, "[EPS COMMAND] Rejected non-OBC source.\n");
            }
            else if (packet->length != sizeof(command_envelope_t)) {
                fprintf(stderr, "[EPS COMMAND] Invalid command size.\n");
            }
            else {
                command_envelope_t command;
                memcpy(&command, packet->data, sizeof(command));

                command_ack_t ack = {0};
                ack.ack_command_id = command.command_id;
                ack.ack_seq = command.seq;
                ack.status = NACK;

                switch (command.command_id) {
                    case EPS_WIRE_COMMAND_REQUEST_TELEMETRY:
                        if (xTelemetryHandle != NULL) {
                            telemetry_request_send();
                            ack.status = ACK;

                            printf(
                                "[EPS COMMAND] Telemetry requested; sequence=%lu.\n",
                                (unsigned long)command.seq
                            );
                            fflush(stdout);
                        }
                        break;

                    default:
                        fprintf(
                            stderr,
                            "[EPS COMMAND] Unknown command ID: %u.\n",
                            (unsigned)command.command_id
                        );
                        break;
                }
                send_command_ack(connection, &ack);
            }

            // Every received packet must be released.
            csp_buffer_free(packet);
        }

        csp_close(connection);
    }

    return NULL;
}

void eps_command_handler_init(void)
{
    pthread_t receiver_thread;

    int result = pthread_create(
        &receiver_thread,
        NULL,
        command_receiver,
        NULL
    );

    if (result != 0) {
        fprintf(
            stderr,
            "[EPS COMMAND] Failed to create receiver: %d.\n",
            result
        );
        return;
    }

    pthread_detach(receiver_thread);
}