#ifndef OBC_COMPUTE_H
#define OBC_COMPUTE_H

#include "obc_ipc.h"
#include "obc_compute_protocol.h"
#include <stddef.h>

int dispatch_thread_init(void);
void *dispatch_thread(void *arg);

int wait_for_reply(uint8_t *buf, size_t buf_size);

#endif // OBC_COMPUTE_H