#ifndef OBC_DATA_FILESYSTEM_H
#define OBC_DATA_FILESYSTEM_H

#include <stdint.h>
#include "obc_ipc.h"

/* Reads path and streams it back to 'requester' as a sequence of data_read_reply_t messages over IPC, ending with is_last=1. On open failure, sends a single status=-1, is_last=1 reply instead. */
void filesystem_stream_file(const char *path, OBC_Roles_t requester);

/* Writes one chunk of `path` (offset==0 truncates/creates, offset>0
   appends -- each chunk is a fully self-contained fopen/write/fclose, no
   state persists across chunks) and sends one data_write_ack_t back to
   `requester`. */
void filesystem_write_chunk(const char *path, uint32_t offset, uint16_t length,
                             const uint8_t *payload, OBC_Roles_t requester);

#endif // OBC_DATA_FILESYSTEM_H