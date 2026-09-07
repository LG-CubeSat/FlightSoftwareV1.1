#ifndef OBC_DATA_FILESYSTEM_H
#define OBC_DATA_FILESYSTEM_H

#include "obc_ipc.h"

/* Reads path and streams it back to 'requester' as a sequence of data_read_reply_t messages over IPC, ending with is_last=1. On open failure, sends a single status=-1, is_last=1 reply instead. */
void filesystem_stream_file(const char *path, OBC_Roles_t requester);

#endif // OBC_DATA_FILESYSTEM_H