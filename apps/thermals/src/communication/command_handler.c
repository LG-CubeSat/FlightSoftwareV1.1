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

                thermal_command_t cmd;
                memcpy(&cmd, packet->data, sizeof(cmd));

                CommandMessage_t msg = {
                    .command = cmd.envelope.command_id,
                    .parameter = cmd.target_temp
                };

                printf("[THERMALS COMMAND HANDLER] Received command=%u parameter=%.2f\n",
                       (unsigned int)msg.command,
                       msg.parameter);
                fflush(stdout);

                if (!command_task_send(&msg))
                {
                    printf(
                        "[THERMALS COMMAND HANDLER] Failed to queue command %u\n",
                        (unsigned int)msg.command
                    );
                    fflush(stdout);
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
