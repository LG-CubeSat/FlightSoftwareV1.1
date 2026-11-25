#include "communication/command_handler.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include <csp/csp.h>
#include "csp_commands.h"

// Keep the socket alive for the lifetime of the program.
static csp_socket_t command_socket = {0};

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

                switch (command.command_id) {
                    case EPS_WIRE_COMMAND_REQUEST_TELEMETRY:
                        printf(
                            "[EPS COMMAND] Telemetry requested; sequence=%lu.\n",
                            (unsigned long)command.seq
                        );
                        fflush(stdout);
                        break;

                    default:
                        fprintf(
                            stderr,
                            "[EPS COMMAND] Unknown command ID: %u.\n",
                            (unsigned)command.command_id
                        );
                        break;
                }
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