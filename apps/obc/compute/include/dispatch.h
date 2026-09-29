#ifndef OBC_COMPUTE_H
#define OBC_COMPUTE_H

#include <stddef.h>
#include <time.h>

#include "obc_ipc.h"
#include "obc_compute_protocol.h"

#define WAIT_REPLY_TOO_BIG (-1)
#define WAIT_REPLY_TIMEOUT (-2)

int dispatch_thread_init(void);
void *dispatch_thread(void *arg);

int wait_for_reply(uint8_t *buf, size_t buf_size, const struct timespec *abs_deadline);

#endif // OBC_COMPUTE_H