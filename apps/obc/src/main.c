#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <csp/csp.h>

#include "csp_network.h"
#include "csp_commands.h"

#define POSITION_STEP      (10)
#define POSITION_WRAP      (50)
#define DEFAULT_PERIOD_SEC (30)

// Listens for telemetry pushed back by the ADCS in response to a position
// command, and prints it -- this is what closes the loop end to end.
static void * telemetry_rx_loop(void * param)
{
    (void) param;

    csp_socket_t sock = {0};
    if (csp_bind(&sock, ADCS_TELEM_PORT) != CSP_ERR_NONE) {
        fprintf(stderr, "OBC: csp_bind on telemetry port failed\n");
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
            if (csp_conn_dport(conn) == ADCS_TELEM_PORT &&
                packet->length >= sizeof(position_telemetry_t)) {

                position_telemetry_t telem;
                memcpy(&telem, packet->data, sizeof(telem));

                printf("[OBC] Telemetry: ADCS reports position=%d\n", telem.current_position);
                fflush(stdout);
            }

            csp_buffer_free(packet);
        }

        csp_close(conn);
    }

    return NULL;
}

// Overridable so the integration test can run the same binary at a fast
// cadence instead of waiting out the real 30s period.
static int get_period_sec(void)
{
    const char * env = getenv("OBC_POSITION_PERIOD_SEC");
    if (env == NULL) {
        return DEFAULT_PERIOD_SEC;
    }

    int period = atoi(env);
    return (period > 0) ? period : DEFAULT_PERIOD_SEC;
}

int main(void)
{
    printf("OBC On...\n");
    fflush(stdout);

    csp_network_init(OBC_ADDRESS, /* is_master = */ 1);

    pthread_t rx_thread;
    if (pthread_create(&rx_thread, NULL, telemetry_rx_loop, NULL) != 0) {
        fprintf(stderr, "OBC: telemetry rx thread create failed\n");
        return 1;
    }

    int period_sec = get_period_sec();
    int32_t target_position = 0;
    uint32_t seq = 0;

    while (1) {
        target_position += POSITION_STEP;
        if (target_position > POSITION_WRAP) {
            target_position = 0;
        }

        csp_conn_t * conn = csp_connect(CSP_PRIO_NORM, ADCS_ADDRESS, ADCS_CMD_PORT, 1000, CSP_O_NONE);
        if (conn == NULL) {
            printf("[OBC] Failed to connect to ADCS\n");
            fflush(stdout);
        } else {
            csp_packet_t * packet = csp_buffer_get(0);
            if (packet == NULL) {
                printf("[OBC] Failed to get CSP buffer\n");
                fflush(stdout);
                csp_close(conn);
            } else {
                position_command_t cmd = {
                    .envelope = { .command_id = CMD_MOVE_TO_POSITION, .seq=seq }, 
                    .target_position = target_position 
                };
                memcpy(packet->data, &cmd, sizeof(cmd));
                packet->length = sizeof(cmd);

                csp_send(conn, packet);

                while ((packet = csp_read(conn, 50)) != NULL) {
                    if (packet->length >= sizeof(command_ack_t)) {
                        
                        command_ack_t command_ack;
                        memcpy(&command_ack, packet->data, sizeof(command_ack));
                        
                        // NACK is 1, because it represents wire being pulled high
                        if (command_ack.status == NACK) {
                            printf("[OBC] Received NACK - Sequence: %d, Command ID: %d\n", seq, command_ack.ack_command_id);
                        }

                        printf("[OBC] Received ACK - Sequence: %d, Command ID: %d\n", seq, command_ack.ack_command_id);
                        fflush(stdout);
                    }
                    csp_buffer_free(packet); // don't forget this whenever you use packet
                }
                csp_close(conn);
                seq++;
                
                printf("[OBC] Sending position command: target=%d\n", target_position);
                fflush(stdout);
            }
        }

        sleep(period_sec);
    }

    return 0;
}
