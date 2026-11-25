#include "communication/command_handler.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include <csp/csp.h>
#include "csp_commands.h"

#include "tasks/telemetry_task.h"

// Keep the socket alive for the lifetime of the program.
static csp_socket_t command_socket = {0};

/* Recent accepted requests are idempotent within this EPS boot. */
#define ACCEPTED_COMMAND_HISTORY (32U)
static command_envelope_t accepted_commands[ACCEPTED_COMMAND_HISTORY];
static size_t accepted_count;
static size_t accepted_next;

static command_ack_status_t accept_command(const command_envelope_t *command)
{
    if (command->command_id != EPS_WIRE_COMMAND_REQUEST_TELEMETRY) {
        return NACK;
    }
    for (size_t i = 0; i < accepted_count; ++i) {
        if (accepted_commands[i].command_id == command->command_id &&
            accepted_commands[i].seq == command->seq) {
            return ACK; /* A retry must not trigger the action again. */
        }
    }
    if (!telemetry_request_send()) {
        return NACK;
    }
    accepted_commands[accepted_next] = *command;
    accepted_next = (accepted_next + 1U) % ACCEPTED_COMMAND_HISTORY;
    if (accepted_count < ACCEPTED_COMMAND_HISTORY) {
        ++accepted_count;
    }
    return ACK;
}

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

        /* Bound how long one client can monopolize this receiver. */
        unsigned packets_processed = 0;
        while (packets_processed++ < 8U &&
               (packet = csp_read(connection, 100)) != NULL) {
            if (csp_conn_src(connection) != OBC_ADDRESS) {
                fprintf(stderr, "[EPS COMMAND] Rejected non-OBC source.\n");
            }
            else if (packet->length < sizeof(command_envelope_t)) {
                fprintf(stderr, "[EPS COMMAND] Invalid command size.\n");
            }
            else {
                command_envelope_t command;
                memcpy(&command, packet->data, sizeof(command));

                command_ack_t ack = {0};
                ack.ack_command_id = command.command_id;
                ack.ack_seq = command.seq;
                ack.status = NACK;

                if (packet->length == sizeof(command)) {
                    ack.status = accept_command(&command);
                }
                send_command_ack(connection, &ack);
                printf("[EPS COMMAND] ID=%u sequence=%lu: %s\n",
                       (unsigned)command.command_id, (unsigned long)command.seq,
                       ack.status == ACK ? "ACK" : "NACK");
                fflush(stdout);
            }

            // Every received packet must be released.
            csp_buffer_free(packet);
        }

        csp_close(connection);
    }

    return NULL;
}

int eps_command_handler_init(void)
{
    if (csp_bind(&command_socket, EPS_CMD_PORT) != CSP_ERR_NONE ||
        csp_listen(&command_socket, 5) != CSP_ERR_NONE) {
        fprintf(stderr, "[EPS COMMAND] Failed to bind/listen on command port.\n");
        return -1;
    }
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
        return -1;
    }

    pthread_detach(receiver_thread);
    return 0;
}
