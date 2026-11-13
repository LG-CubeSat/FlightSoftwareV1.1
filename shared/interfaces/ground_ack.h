#ifndef GROUND_ACK_H
#define GROUND_ACK_H

#include <stddef.h>
#include <stdint.h>

/* Radio payload inside an ordinary raw-telemetry CCSDS frame. The sequence
 * matches the command envelope's sequence, which the GS uses as request ID. */
#define GROUND_ACK_WIRE_SIZE 14U

typedef enum {
    GROUND_ACK_ACCEPTED = 0,   /* ADCS command handler ACKed */
    GROUND_ACK_REJECTED = 1,   /* ADCS command handler NACKed */
    GROUND_ACK_TIMEOUT = 2,    /* no CSP reply from board */
    GROUND_ACK_ROUTE_ERROR = 3,/* OBC could not route the command */
    GROUND_ACK_OBC_QUEUED = 4  /* OBC accepted a local action, not completed */
} ground_ack_status_t;

static inline void ground_ack_encode(uint8_t out[GROUND_ACK_WIRE_SIZE],
                                     uint8_t node, uint8_t port,
                                     uint8_t command_id, uint32_t sequence,
                                     ground_ack_status_t status) {
    out[0] = 'W'; out[1] = 'A'; out[2] = 'C'; out[3] = 'K';
    out[4] = 1;
    out[5] = node;
    out[6] = port;
    out[7] = command_id;
    out[8] = (uint8_t)status;
    out[9] = 0;
    out[10] = (uint8_t)(sequence >> 24);
    out[11] = (uint8_t)(sequence >> 16);
    out[12] = (uint8_t)(sequence >> 8);
    out[13] = (uint8_t)sequence;
}

#endif
