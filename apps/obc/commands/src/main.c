#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "relay.h"
#include "ingest.h"
#include "heartbeat.h"
#include "csp_network.h"
#include <csp/csp.h>
#include "radio.h"
#include "obc_ipc.h"
#include "obc_relay_protocol.h"
#include "commands_health.h"
#include "obc_log.h"
#include "csp_commands.h"

#define RADIO_RX_BUF_SIZE 256

static void *radio_rx_thread(void *arg) {
    (void)arg;
    uint8_t buffer[RADIO_RX_BUF_SIZE];
    printf("[OBC RADIO RX] Radio receiver thread started.\n");
    fflush(stdout);

    for (;;) {
        // Blocks on UART until a valid CCSDS packet arrives and CRC-32 is verified
        int length = radio_receive(buffer, sizeof(buffer));
        if (length <= 0) {
            continue; // timeout or quiet line
        }

        printf("[OBC RADIO RX] Received RF Packet from ground: %d bytes\n", length);
        fflush(stdout);

        // ground telecommand wire format:
        // byte 0: destination CSP node (e.g., ADCS_ADDRESS = 2)
        // byte 1: destination CSP port (e.g. ADCS_CMD_PORT = 10)
        // bytes 2..N: command payload
        if (length < 2) {
            fprintf(stderr, "[OBC RADIO RX] Packet too short (%d bytes), dropping\n", length);
            continue;
        }

        uint8_t dest_addr = buffer[0];
        uint8_t dest_port = buffer[1];
        uint16_t payload_len = (uint16_t)(length - 2);
        const uint8_t *payload = &buffer[2];

        printf("[OBC RADIO RX] Routing command to Node %u, Port %u (%u payload bytes)\n", dest_addr, dest_port, payload_len);
        fflush(stdout);

        // check if this command is intended for the OBC itself
        if (dest_addr == OBC_ADDRESS) {
            if (payload_len >= sizeof(command_envelope_t)) {
                const command_envelope_t *env = (const command_envelope_t *)payload;
                if (env->command_id == CMD_TAKE_PHOTO) {
                    printf("[OBC RADIO RX] Uplink CMD_TAKE_PHOTO -> Forwarding to ROLE_MISSION\n");
                    fflush(stdout);
                    IPC_send(ROLE_MISSION, payload, payload_len);
                } else {
                    fprintf(stderr, "[OBC RADIO RX] Unknown OBC command ID: %u\n", env->command_id);
                }
            } else {
                fprintf(stderr, "[OBC RADIO RX] OBC command payload too short (%u bytes)\n", payload_len);
            }
            continue;
        }

        // forward over CSP bus (I2C to ADCS)
        csp_conn_t *conn = csp_connect(CSP_PRIO_NORM, dest_addr, dest_port, 1000, CSP_O_NONE);
        if (conn == NULL) {
            fprintf(stderr, "[OBC RADIO RX] Failed to connect to CSP node %u port %u\n", dest_addr, dest_port);
            continue;
        }

        csp_packet_t *csp_pkt = csp_buffer_get(0);
        if (csp_pkt == NULL) {
            fprintf(stderr, "[OBC RADIO RX] Out of CSP packet buffers\n");
            csp_close(conn);
            continue;
        }

        memcpy(csp_pkt->data, payload, payload_len);
        csp_pkt->length = payload_len;
        csp_send(conn, csp_pkt);
        csp_close(conn);

        printf("[OBC RADIO RX] Forwarded successfully.\n");
        fflush(stdout);
    }
    return NULL;
}

static int start_radio_rx_thread(void) {
    pthread_t thread;
    return pthread_create(&thread, NULL, radio_rx_thread, NULL);
}

int main(void) {
    if (obc_log_init("command") != 0) {
        fprintf(stderr, "[OBC COMMAND] Failed to initialize logging.\n");
    }

    LOG_INFO("Initializing");
    printf("[OBC COMMAND] Program started.\n");

    /*
    init stuff here
    */
    csp_network_init(OBC_ADDRESS, 1); // is master
    if (IPC_initialize(ROLE_COMMANDS) != IPC_OK) {
        fprintf(stderr, "[OBC COMMANDS] Failed to initialize IPC.\n");
        return 1;
    }

    if (commands_health_init() != 0) {
        fprintf(stderr, "[OBC COMMANDS] Failed to initialize health tracking.\n");
        return 1;
    }

    if (ingest_thread_init() != 0 || relay_thread_init() != 0) {
        fprintf(stderr, "[OBC COMMANDS] Failed to start a worker thread.\n");
        return 1;
    }

    if (start_radio_rx_thread() != 0) {
        fprintf(stderr, "[OBC COMMANDS] Failed to start radio received thread.\n");
        return 1;
    }

    if (heartbeat_thread_init() != 0) {
        fprintf(stderr, "[OBC COMMANDS] Failed to start heartbeat thread.\n");
        return 1;
    }

    for (;;) { sleep(1); }

    return 0;
}
