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

static pthread_mutex_t reply_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t reply_cond = PTHREAD_COND_INITIALIZER;
static uint8_t reply_buf[COMPUTE_MAX_MSG_SIZE];
static int reply_len = 0;
static int reply_ready = 0;

static uint8_t input_buf[COMPUTE_MAX_DATA_SIZE];
static uint8_t compressed_buf[COMPUTE_COMPRESSED_CAP];

/* Called only by dispatch_thread, when a message arrives from ROLE_DATE. */
static void deliver_reply(const uint8_t *buf, int len) {
    pthread_mutex_lock(&reply_lock);
    while (reply_ready) {
        pthread_cond_wait(&reply_cond, &reply_lock); // wait if worker hasn't consumed the last one yet.
    }
    memcpy(reply_buf, buf, (size_t)len);
    reply_len = len;
    reply_ready = 1;
    pthread_cond_signal(&reply_cond); // wake the worker
    pthread_mutex_unlock(&reply_lock);
}

static int wait_for_reply(uint8_t *buf, size_t buf_size) {
    pthread_mutex_lock(&reply_lock);
    while (!reply_ready) {
        pthread_cond_wait(&reply_cond, &reply_lock); // sleep until deliver_reply signals
    }
    int len = reply_len;
    if ((size_t)len <= buf_size) {
        memcpy(buf, reply_buf, (size_t)len);
    } else {
        reply_ready = 0;
        pthread_cond_signal(&reply_cond); // wake deliver_reply if it's waiting for the slot to free up
        pthread_mutex_unlock(&reply_lock);
        return len;
    }
}

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
    worker_job_t *job = (worker_job_t *)arg;
    uint32_t job_id = job->req.job_id;
    OBC_Roles_t requester = job->requester;
    char in_path[COMPUTE_MAX_PATH];
    char out_path[COMPUTE_MAX_PATH];

    /* --- phase 1: read in_path from data, in chunks --- */
    data_read_request_t read_req = {0};
    snprintf(read_req.path, sizeof(read_req.path), "%s", in_path);
    IPC_send(ROLE_DATA, (const uint8_t *)&read_req, sizeof(read_req));

    size_t input_len = 0;
    for (;;) {
        uint8_t buf[sizeof(data_read_reply_t)];
        int len = wait_for_reply(buf, sizeof(buf));
        if (len != sizeof(data_read_reply_t)) continue;

        data_read_reply_t reply;
        memcpy(&reply, buf, sizeof(reply));

        if (reply.status != 0) {
            compute_result_t result = { .job_id = job_id, .status = COMPUTE_STATUS_FAILED };
            IPC_send(requester, (const uint8_t *)&result, sizeof(result));

            pthread_mutex_lock(&job_lock); job_busy = 0;
            pthread_mutex_unlock(&job_lock);
            return NULL;
        }

        memcpy(input_buf + input_len, reply.payload, reply.length);
        input_len += reply.length;
        if (reply.is_last) break;
    }

    size_t compressed_len = 0;
    int image_id = image_id_counter++;
    if (ssdv_encode_image(input_buf, input_len, CALL_SIGN, (uint8_t)image_id, compressed_buf, sizeof(compressed_buf), &compressed_len) != 0) {
        compute_result_t result = { .job_id = job_id, .status = COMPUTE_STATUS_FAILED };
        IPC_send(requester, (const uint8_t *)&result, sizeof(result));
        pthread_mutex_lock(&job_lock); job_busy = 0;
        pthread_mutex_unlock(&job_lock);
        return NULL;
    }

    size_t written = 0;
    while (written < compressed_len) {
        size_t chunk_len = compressed_len - written;
        if (chunk_len > DATA_WRITE_CHUNK_SIZE) chunk_len = DATA_WRITE_CHUNK_SIZE;

        data_write_chunk_t chunk = {0};
        snprintf(chunk.path, sizeof(chunk.path), "%s", out_path);
        chunk.offset = (uint32_t)written;
        chunk.length = (uint16_t)chunk_len;
        chunk.is_last = (written + chunk_len >= compressed_len) ? 1 : 0;
        memcpy(chunk.payload, compressed_buf + written, chunk_len);
        IPC_send(ROLE_DATA, (const uint8_t *)&chunk, sizeof(chunk));

        uint8_t ack_buf[sizeof(data_write_ack_t)];
        int len = wait_for_reply(ack_buf, sizeof(ack_buf));
        data_write_ack_t ack;
        if (len != sizeof(ack) || (memcpy(&ack, ack_buf, sizeof(ack)), ack.status != 0)) {
            compute_result_t result = { .job_id = job_id, .status = COMPUTE_STATUS_FAILED };
            IPC_send(requester, (const uint8_t *)&result, sizeof(result));
            pthread_mutex_lock(&job_lock); job_busy=0;
            pthread_mutex_unlock(&job_lock);
            return NULL;
        }
        written += chunk_len;
    }
    compute_result_t result = { .job_id = job_id, .status = COMPUTE_STATUS_OK, .output_size = (uint32_t)compressed_len };
    IPC_send(requester, (const uint8_t *)&result, sizeof(result));
    pthread_mutex_unlock(&job_lock);
    return NULL;
}

void *dispatch_thread(void *arg) {
    (void)arg;
    uint8_t buf[COMPUTE_MAX_MSG_SIZE];

    for (;;) {
        OBC_Roles_t src;
        int len = IPC_receive(&src, buf, sizeof(buf));

        if (len < 0) continue;

        printf("[OBC COMPUTE] got %d bytes from role %d\n", len, src);

        if (src == ROLE_DATA) {
            deliver_reply(buf, len);
        } else if (len == sizeof(compute_compress_request_t)) {
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