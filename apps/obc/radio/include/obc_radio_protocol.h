#ifndef OBC_RADIO_PROTOCOL_H
#define OBC_RADIO_PROTOCOL_H

#include <stdint.h>
#include "obc_ipc.h"
#include "radio.h"

typedef enum {
    OBC_RADIO_SEND_RAW = 1,
    OBC_RADIO_SEND_SSDV = 2,
    OBC_RADIO_UPLINK = 3,
    OBC_RADIO_RESULT = 4
} obc_radio_kind_t;

/* Header followed by length payload bytes. All OBC roles use the same ABI. */
typedef struct {
    uint8_t kind;
    uint8_t status; /* RESULT: 0 = sent, 1 = failed */
    uint16_t length;
    uint32_t request_id; /* zero means no result requested */
    uint8_t payload[];
} obc_radio_message_t;

#define OBC_RADIO_HEADER_SIZE 8U
#define OBC_RADIO_MAX_DATA RADIO_MAX_PAYLOAD

_Static_assert(sizeof(obc_radio_message_t) == OBC_RADIO_HEADER_SIZE, "radio IPC header size");
_Static_assert(OBC_RADIO_HEADER_SIZE + OBC_RADIO_MAX_DATA <= OBC_IPC_MAX_PAYLOAD, "radio IPC payload size");

#endif
