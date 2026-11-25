#include "eps_command_client.h"

#include <csp/csp.h>
#include <string.h>

eps_command_result_t eps_command_send(const void *payload, size_t length)
{
    if (payload == NULL || length != sizeof(command_envelope_t)) {
        return EPS_COMMAND_INVALID_REQUEST;
    }

    command_envelope_t command;
    memcpy(&command, payload, sizeof(command));

    csp_conn_t *connection = csp_connect(
        CSP_PRIO_NORM, EPS_ADDRESS, EPS_CMD_PORT, 1000, CSP_O_NONE);
    if (connection == NULL) {
        return EPS_COMMAND_CONNECT_FAILED;
    }

    csp_packet_t *packet = csp_buffer_get(0);
    if (packet == NULL) {
        csp_close(connection);
        return EPS_COMMAND_NO_BUFFER;
    }

    _Static_assert(sizeof(command_envelope_t) <= CSP_BUFFER_SIZE,
                   "EPS command exceeds CSP packet capacity");
    memcpy(packet->data, payload, length);
    packet->length = (uint16_t)length;
    csp_send(connection, packet); /* CSP now owns packet. */

    eps_command_result_t result = EPS_COMMAND_TIMEOUT;
    csp_packet_t *reply = csp_read(connection, 1000);
    if (reply != NULL) {
        result = EPS_COMMAND_INVALID_REPLY;
        if (reply->length == sizeof(command_ack_t)) {
            command_ack_t ack;
            memcpy(&ack, reply->data, sizeof(ack));
            if (ack.ack_command_id == command.command_id &&
                ack.ack_seq == command.seq) {
                if (ack.status == ACK) {
                    result = EPS_COMMAND_ACCEPTED;
                } else if (ack.status == NACK) {
                    result = EPS_COMMAND_REJECTED;
                }
            }
        }
        csp_buffer_free(reply);
    }
    csp_close(connection);
    return result;
}

const char *eps_command_result_name(eps_command_result_t result)
{
    switch (result) {
        case EPS_COMMAND_ACCEPTED: return "ACK (accepted, not delivered)";
        case EPS_COMMAND_REJECTED: return "NACK (rejected)";
        case EPS_COMMAND_INVALID_REQUEST: return "invalid request size";
        case EPS_COMMAND_CONNECT_FAILED: return "connection unavailable";
        case EPS_COMMAND_NO_BUFFER: return "no packet buffer";
        case EPS_COMMAND_TIMEOUT: return "acknowledgment timeout";
        default: return "invalid acknowledgment";
    }
}
