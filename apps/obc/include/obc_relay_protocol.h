#ifndef OBC_RELAY_PROTOCOL_H
#define OBC_RELAY_PROTOCOL_H

#include <stdint.h>

typedef struct {
    uint8_t dest_addr; // CSP node address, e.g. ADCS_ADDRESS
    uint8_t dest_port; // CSP port, e.g. ADCS_CMD_PORT
    uint16_t length;
    uint8_t payload[128];
} relay_request_t;

/* Returned only for EPS command requests; result uses eps_command_result_t.
 * A zero command ID/sequence means the request was too short to identify.
 */
#define EPS_RELAY_REPLY_MAGIC UINT32_C(0x45505352)
typedef struct {
    uint32_t magic;
    uint32_t sequence;
    uint8_t command_id;
    int8_t result;
} eps_relay_reply_t;

#endif
