#ifndef OBC_DATA_PROTOCOL_H
#define OBC_DATA_PROTOCOL_H

#include <stdint.h>

#define DATA_MAX_PATH 64
#define DATA_CHUNK_SIZE 200 // headroom under obc_ipc's 256-byte MAX_IPC_PAYLOAD

/* role -> ROLE_DATA: "read this file back to me, in chunks" */
typedef struct {
    char path[DATA_MAX_PATH];
} data_read_request_t;

/* ROLE_DATA -> requester, one or more of these per request */
typedef struct {
    int status;
    uint32_t offset;
    uint16_t length;
    uint8_t is_last;
    uint8_t payload[DATA_CHUNK_SIZE];
} data_read_reply_t;

#endif