/*
Receives commands such as "Point to Earth" or "Reset"

Runs a small CSP listener on ADCS_CMD_PORT. Decodes incoming
position_command_t payloads and hands them to the Command task's
queue -- this file's only job is CSP receive + decode.

TODO: ADAPT TO EPS (what info im sending)
*/
#include "../../include/communication/command_handler.h"

#include <csp/csp.h>

#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "csp_commands.h"
#include "../../include/tasks/command_task.h"

static void * command_handler_rx_loop(void * param)
{
    (void) param;

    csp_socket_t sock = {0};
    if (csp_bind(&sock, EPS_CMD_PORT) != CSP_ERR_NONE) {
        fprintf(stderr, "[COMMAND HANDLER] csp_bind failed\n");
        fflush(stderr);
        return NULL;
    }
    csp_listen(&sock, 5);

    while (1) {
        csp_conn_t * conn = csp_accept(&sock, 10000);
        if (conn == NULL) {
            continue; /* accept() timeout, try again */
        }

        csp_packet_t * packet;
        while ((packet = csp_read(conn, 50)) != NULL) {
            if (csp_conn_dport(conn) == EPS_CMD_PORT &&
                packet->length >= sizeof(position_command_t)) {

                position_command_t cmd;
                memcpy(&cmd, packet->data, sizeof(cmd));

                printf("[COMMAND HANDLER] Position command received: target=%d\n", cmd.target_position);
                fflush(stdout);

                CommandMessage_t msg = {
                    .command = CMD_MOVE_TO_POSITION,
                    .parameter = (uint32_t)cmd.target_position
                };
                command_task_send(&msg);
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
    if (ret != 0) {
        fprintf(stderr, "[COMMAND HANDLER] rx thread create failed: %d\n", ret);
        fflush(stderr);
    }
}
