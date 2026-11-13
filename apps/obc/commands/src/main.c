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
#include "ground_ack.h"

#define RADIO_RX_BUF_SIZE 256

static void downlink_ack(uint8_t node, uint8_t port, uint8_t command_id,
                         uint32_t sequence, ground_ack_status_t status) {
    uint8_t wire[GROUND_ACK_WIRE_SIZE];
    ground_ack_encode(wire, node, port, command_id, sequence, status);
    if (radio_send(wire, sizeof(wire)) != 0) {
        fprintf(stderr, "[OBC RADIO RX] Could not queue command result for seq %u\n", sequence);
    }
}

static void *radio_rx_thread(void *arg) {
    (void)arg;
    uint8_t buffer[RADIO_RX_BUF_SIZE];
    printf("[OBC RADIO RX] Radio receiver thread started.\n");
    fflush(stdout);

    for (;;) {
        // The radio verifies CCSDS framing and CRC, then returns the command payload.
        int length = radio_receive(buffer, sizeof(buffer));
        if (length <= 0) {
            continue; // timeout or quiet line
        }

        printf("[OBC RADIO RX] Received RF command payload: %d bytes\n", length);
        fflush(stdout);

        // ground telecommand wire format:
        // byte 0: destination CSP node (e.g., ADCS_ADDRESS = 2)
        // byte 1: destination CSP port (e.g. ADCS_CMD_PORT = 10)
        // bytes 2..N: command payload
        if (length < 2 + (int)sizeof(command_envelope_t)) {
            fprintf(stderr, "[OBC RADIO RX] Command payload too short (%d bytes), dropping\n",
                    length);
            continue;
        }

        uint8_t dest_addr = buffer[0];
        uint8_t dest_port = buffer[1];
        if (!((dest_addr == OBC_ADDRESS && dest_port == 0) ||
              (dest_addr == ADCS_ADDRESS && dest_port == ADCS_CMD_PORT) ||
              (dest_addr == EPS_ADDRESS && dest_port == EPS_CMD_PORT) ||
              (dest_addr == THERMALS_ADDRESS && dest_port == THERMALS_CMD_PORT))) {
            fprintf(stderr, "[OBC RADIO RX] Dropping payload with unknown command route %u:%u\n",
                    dest_addr, dest_port);
            continue;
        }
        uint16_t payload_len = (uint16_t)(length - 2);
        const uint8_t *payload = &buffer[2];
        command_envelope_t envelope;
        memcpy(&envelope, payload, sizeof(envelope));

        printf("[OBC RADIO RX] Routing command to Node %u, Port %u (%u payload bytes)\n", dest_addr, dest_port, payload_len);
        fflush(stdout);

        // check if this command is intended for the OBC itself
        if (dest_addr == OBC_ADDRESS) {
            if (envelope.command_id == CMD_TAKE_PHOTO) {
                printf("[OBC RADIO RX] Uplink CMD_TAKE_PHOTO -> Forwarding to ROLE_MISSION\n");
                fflush(stdout);
                int queued = IPC_send(ROLE_MISSION, payload, payload_len);
                downlink_ack(dest_addr, dest_port, envelope.command_id,
                             envelope.seq, queued >= 0 ? GROUND_ACK_OBC_QUEUED : GROUND_ACK_ROUTE_ERROR);
            } else {
                fprintf(stderr, "[OBC RADIO RX] Unknown OBC command ID: %u\n", envelope.command_id);
                downlink_ack(dest_addr, dest_port, envelope.command_id,
                             envelope.seq, GROUND_ACK_REJECTED);
            }
            continue;
        }

        // forward over CSP bus (I2C to ADCS)
        csp_conn_t *conn = csp_connect(CSP_PRIO_NORM, dest_addr, dest_port, 1000, CSP_O_NONE);
        if (conn == NULL) {
            fprintf(stderr, "[OBC RADIO RX] Failed to connect to CSP node %u port %u\n", dest_addr, dest_port);
            downlink_ack(dest_addr, dest_port, envelope.command_id,
                         envelope.seq, GROUND_ACK_ROUTE_ERROR);
            continue;
        }

        csp_packet_t *csp_pkt = csp_buffer_get(0);
        if (csp_pkt == NULL) {
            fprintf(stderr, "[OBC RADIO RX] Out of CSP packet buffers\n");
            csp_close(conn);
            downlink_ack(dest_addr, dest_port, envelope.command_id,
                         envelope.seq, GROUND_ACK_ROUTE_ERROR);
            continue;
        }

        memcpy(csp_pkt->data, payload, payload_len);
        csp_pkt->length = payload_len;
        csp_send(conn, csp_pkt);

        ground_ack_status_t result = GROUND_ACK_TIMEOUT;
        csp_packet_t *reply = csp_read(conn, 1000);
        if (reply != NULL) {
            command_ack_t ack;
            if (reply->length >= sizeof(ack)) {
                memcpy(&ack, reply->data, sizeof(ack));
                if (ack.ack_command_id == envelope.command_id && ack.ack_seq == envelope.seq &&
                    (ack.status == ACK || ack.status == NACK)) {
                    result = ack.status == ACK ? GROUND_ACK_ACCEPTED : GROUND_ACK_REJECTED;
                } else {
                    result = GROUND_ACK_ROUTE_ERROR;
                }
            } else {
                result = GROUND_ACK_ROUTE_ERROR;
            }
            csp_buffer_free(reply);
        }
        csp_close(conn);

        downlink_ack(dest_addr, dest_port, envelope.command_id, envelope.seq, result);

        printf("[OBC RADIO RX] Command seq %u result %u.\n", envelope.seq, result);
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
