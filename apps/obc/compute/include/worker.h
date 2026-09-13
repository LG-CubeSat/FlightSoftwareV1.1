#ifndef WORKER_H
#define WORKER_H

#include <stdint.h>

#include "obc_ipc.h"
#include "obc_compute_protocol.h"

typedef struct {
    compute_compress_request_t req;
    OBC_Roles_t requester;
} worker_job_t;

void handle_compress_request(const uint8_t *buf, OBC_Roles_t role);
void handle_cancel_request(const uint8_t *buf);

#endif