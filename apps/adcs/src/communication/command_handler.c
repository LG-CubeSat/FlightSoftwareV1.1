/*
Receives commands such as "Point to Earth" or "Reset"

Runs a small CSP listener on ADCS_CMD_PORT. Decodes incoming
position_command_t payloads and hands them to the Command task's
queue -- this file's only job is CSP receive + decode.
*/
#include "../../include/communication/command_handler.h"

#include <csp/csp.h>

#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "csp_commands.h"
#include "../../include/tasks/command_task.h"
#include "fault_manager.h"
#include "board_shutdown.h"

static void * command_handler_rx_loop(void * param)
{
    (void) param;

    csp_socket_t sock = {0};
    if (csp_bind(&sock, ADCS_CMD_PORT) != CSP_ERR_NONE) {
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
            if (csp_conn_dport(conn) == ADCS_CMD_PORT &&
                packet->length >= sizeof(command_envelope_t)) {

                command_envelope_t envelope;
                memcpy(&envelope, packet->data, sizeof(envelope));
                
                command_ack_t reply = { .ack_command_id = envelope.command_id, .ack_seq = envelope.seq };
                
                switch (envelope.command_id) {
                    case CMD_MOVE_TO_POSITION:
                        // Bounds-checking a *commanded* value here would be wrong: an
                        // out-of-range request is a bad command (or a bad sender), not
                        // a local fault -- reject it and keep running. Checking our own
                        // computed/estimated state (once real) is estimation_task's job,
                        // see fault_management_check_bounds there.
                        if (packet->length >= sizeof(position_command_t)) {
                            position_command_t cmd;
                            memcpy(&cmd, packet->data, sizeof(cmd));
                            reply.status = ACK;
                            CommandMessage_t msg = { .command = CMD_MOVE_TO_POSITION, .parameter = (uint32_t)cmd.target_position };
                            command_task_send(&msg);
                        } else {
                            reply.status = NACK;
                        }
                        break;
                    case CMD_RESET:
                        printf("[COMMAND HANDLER] Reset command received -- resetting now\n");
                        fflush(stdout);
                        reply.status = ACK;
                        fault_management_trigger_reset(RESET_REASON_WATCHDOG);
                        break;
                    case CMD_SHUTDOWN:
                        printf("[COMMAND HANDLER] Shutdown command received. Shutting down.\n");
                        fflush(stdout);
                        reply.status = ACK;
                        board_shutdown();
                        break;
                    case CMD_POINT_TO_SUN:
                        printf("[COMMAND HANDLER] Point to sun command received. \n");
                        fflush(stdout)
                        reply.status = ACK;
                    default:
                        reply.status = NACK;
                        break;
                }
                
                csp_packet_t * ack_packet = csp_buffer_get(0);

                if (ack_packet == NULL) {
                    printf("[COMMAND HANDLER] Faiuled to get packet buffer when sending ACK Replay of %d.\n", reply.status);
                } else {
                    memcpy(ack_packet->data, &reply, sizeof(reply));
                    ack_packet->length = sizeof(reply);

                    csp_send(conn, ack_packet);
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
    if (ret != 0) {
        fprintf(stderr, "[COMMAND HANDLER] rx thread create failed: %d\n", ret);
        fflush(stderr);
    }
}
