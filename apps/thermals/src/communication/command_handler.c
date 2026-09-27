#include "communication/command_handler.h"

#include <csp/csp.h>

#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "csp_commands.h"
#include "tasks/command_task.h"

static void *command_handler_rx_loop(void *param)
{
    (void)param;

    csp_socket_t sock = {0};

    if (csp_bind(&sock, THERMALS_CMD_PORT) != CSP_ERR_NONE)
    {
        printf("[THERMALS COMMAND HANDLER] csp_bind failed\n");
        fflush(stdout);
        return NULL;
    }

    csp_listen(&sock, 5);

    while (1)
    {
        csp_conn_t *conn = csp_accept(&sock, 10000);
        if (conn == NULL)
        {
            continue;
        }

        csp_packet_t *packet;
        while ((packet = csp_read(conn, 50)) != NULL)
        {
            if (csp_conn_dport(conn) == THERMALS_CMD_PORT &&
                packet->length >= sizeof(thermal_command_t))
            {

            thermal_command_t payload;
            memcpy(&payload, packet->data, sizeof(payload));

            thermals_command_t message = {0};

            message.sequence = payload.envelope.seq;

            switch (payload.envelope.command_id)
            {
                case THERMALS_WIRE_COMMAND_SET_TARGET_TEMP:
                    message.type = THERMALS_COMMAND_SET_TARGET_TEMP;
                    message.parameter.target_temp = payload.target_temp;
                    break;

                case THERMALS_WIRE_COMMAND_REQUEST_TELEMETRY:
                    message.type = THERMALS_COMMAND_REQUEST_TELEMETRY;
                    break;

                default:
                    printf(
                        "[THERMALS COMMAND HANDLER] Unknown wire command: %u\n",
                        (unsigned int)payload.envelope.command_id
                    );
                    fflush(stdout);
                    break;
            }

            if (message.type != THERMALS_COMMAND_NONE)
            {
                printf(
                    "[THERMALS COMMAND HANDLER] Decoded command=%u sequence=%lu\n",
                    (unsigned int)message.type,
                    (unsigned long)message.sequence
                );
                fflush(stdout);

                if (!command_task_send(&message))
                {
                    printf(
                        "[THERMALS COMMAND HANDLER] Failed to queue command %u\n",
                        (unsigned int)message.type
                    );
                    fflush(stdout);
                }
            }
            }

            csp_buffer_free(packet);
        }

        csp_close(conn);
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
}
