#include "compute.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "obc_ipc.h"
#include "obc_compute_protocol.h"
#include "obc_data_protocol.h"
#include "ssdv_codec.h"

#define CALL_SIGN 'A' // TODO: make this fetched from mission process...
#define COMPUTE_MAX_DATA_SIZE (64 * 1024)          // matches payload_commander's MAX_PHOTO_SIZE ceiling
#define COMPUTE_COMPRESSED_CAP (COMPUTE_MAX_DATA_SIZE + 1024) // header + per-block overhead margin
#define COMPUTE_MAX_MSG_SIZE 256                    // matches obc_ipc's own MAX_IPC_PAYLOAD cap

static pthread_mutex_t job_lock = PTHREAD_MUTEX_INITIALIZER;
static int job_busy = 0;
static uint32_t job_id_running = 0;
static volatile int job_cancel_requested = 0; // not required to be volatile, but matches convention

static worker_job_t current_job; // One job at a time, safe to reuse

static int image_id_counter = 0;
static int computing = 0; // 1 means we are computing

int dispatch_thread_init(void) {
    printf("[OBC COMPUTE] Attempting dispatch pthread creation.\n");
    pthread_t dispatch_pthread;
    int ret = pthread_create(&dispatch_pthread, NULL, dispatch_thread, NULL);
    if (ret != 0) {
        printf("[OBC COMPUTE] Failed to create pthread.\n");
    } else {
        printf("[OBC COMPUTE] Successfully create pthread.\n");
    }
    return ret;
}

void *worker_thread(void *arg) {
    (void)arg;
    
    
}

void *dispatch_thread(void *arg) {
    (void)arg;
    uint8_t buf[COMPUTE_MAX_MSG_SIZE];

    for (;;) {
        OBC_Roles_t src;
        int len = IPC_receive(&src, buf, sizeof(buf));

        if (len < 0) continue;

        printf("[OBC COMPUTE] got %d bytes from role %d\n", len, src);

        if (len == sizeof(compute_compress_request_t)) {
            handle_compress_request(buf, src);
        } else if (len == sizeof(compute_cancel_request_t)) {
            handle_cancel_request(buf);
        }
    }
}

void handle_compress_request(const uint8_t *buf, OBC_Roles_t src) {
    compute_compress_request_t req;
    memcpy(&req, buf, sizeof(req));
    req.in_path[sizeof(req.in_path) - 1] = '\0'; // guard against a non-terminated string on a wire
    req.out_path[sizeof(req.out_path) -1] = '\0';

    pthread_mutex_lock(&job_lock);
    if (job_busy) {
        pthread_mutex_unlock(&job_lock);
        printf("[OBC COMPUTE] job %u rejected -- already running job %u\n", req.job_id, job_id_running);

        compute_result_t result = { .job_id = req.job_id, .status = COMPUTE_STATUS_BUSY };
        IPC_send(src, (const uint8_t *)&result, sizeof(result));
        return;
    }
    
    // set the job to busy
    job_busy = 1;
    job_id_running = req.job_id;
    job_cancel_requested = 0;
    pthread_mutex_unlock(&job_lock);

    current_job.req = req;
    pthread_t worker;

    /*
    Making pthread detached means it is on-demand
    It reports its own result over IPC and exits when finished.
    This way it doesn't need to be tied down and can be a job/worker instead of a queried loop
    */
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    int ret = pthread_create(&worker, &attr, worker_thread, &current_job);
    pthread_attr_destroy(&attr);

    if (ret != 0) {
        printf("[OBC COMPUTE] Failed to start worker thread for job %u\n", req.job_id);
        pthread_mutex_lock(&job_lock);
        job_busy = 0;
        pthread_mutex_unlock(&job_lock);
        compute_result_t result = { .job_id = req.job_id, .status = COMPUTE_STATUS_FAILED };
        IPC_send(src, (const uint8_t *)&result, sizeof(result));
    }
}

void handle_cancel_request(const uint8_t *buf) {

}