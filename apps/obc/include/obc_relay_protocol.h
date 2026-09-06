#ifndef OBC_RELAY_PROTOCOL_H
#define OBC_RELAY_PROTOCOL_H

#include <stdint.h>

typedef struct {
    uint8_t dest_addr; // CSP node address, e.g. ADCS_ADDRESS
    uint8_t dest_port; // CSP port, e.g. ADCS_CMD_PORT
    uint16_t length;
    uint8_t payload[128];
} relay_request_t;

#endif