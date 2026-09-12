#ifndef OBC_COMPUTE_H
#define OBC_COMPUTE_H

typedef struct {
    compute_compress_request_t req;
    OBC_Roles_t requester;
} worker_job_t;

int dispatch_thread_init(void);
void *dispatch_thread(void *arg);

#endif // OBC_COMPUTE_H
