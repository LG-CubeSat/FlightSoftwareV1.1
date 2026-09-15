#ifndef OBC_DATA_PROTOCOL_H
#define OBC_DATA_PROTOCOL_H

#include <stdint.h>

#define DATA_MAX_PATH 64
#define DATA_CHUNK_SIZE 200 // headroom under obc_ipc's 256-byte MAX_IPC_PAYLOAD

/* Smaller than DATA_CHUNK_SIZE: a write chunk also carries `path` (writes
   are self-contained per-message, no session state in `data`), which the
   read reply doesn't need. 64(path)+4(offset)+2(length)+1(is_last)+128
   = 199 bytes, safely under the 256-byte cap -- DATA_CHUNK_SIZE (200)
   would have pushed it to 272. */
#define DATA_WRITE_CHUNK_SIZE 128

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

/* role -> ROLE_DATA: write a chunk of `path`. offset==0 truncates/creates,
   offset>0 appends -- no state needs to persist in `data` across chunks,
   same trick the read path uses (each chunk is a fully self-contained
   fopen/write/fclose). Distinguished from data_read_request_t on the wire
   purely by size, same as every other role's IPC dispatch in this codebase. */
typedef struct {
    char path[DATA_MAX_PATH];
    uint32_t offset;
    uint16_t length;
    uint8_t is_last;
    uint8_t payload[DATA_WRITE_CHUNK_SIZE];
} data_write_chunk_t;

/* ROLE_DATA -> requester, one reply per chunk received -- lets the writer
   abort immediately on the first failure instead of blindly finishing a
   doomed transfer. */
typedef struct {
    int status; // 0 = chunk written ok, -1 = error (e.g. open failed)
} data_write_ack_t;

#endif