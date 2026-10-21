#ifndef OBC_COMPUTE_H
#define OBC_COMPUTE_H

#include <stddef.h>
#include <time.h>

#include "obc_ipc.h"
#include "obc_compute_protocol.h"

#define WAIT_REPLY_TOO_BIG (-1)
#define WAIT_REPLY_TIMEOUT (-2)
#define WAIT_REPLY_ABORTED (-3) // the job's epoch closed underneath

int dispatch_thread_init(void);
void *dispatch_thread(void *arg);

int wait_for_reply(uint8_t *buf, size_t buf_size, uint32_t epoch, const struct timespec *abs_deadline);

uint32_t dispatch_job_begin(void); // bump epoch, open it, clear the slot, return the epoch
void dispatch_job_end(uint32_t e); // close it, clear the slot, broadcast



#endif // OBC_COMPUTE_H