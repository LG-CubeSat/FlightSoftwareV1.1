#include "worker.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "pthread.h"
#include "obc_data_protocol.h"
#include "ssdv_codec.h"
#include "dispatch.h"
#include "time.h"

#define CALL_SIGN "COM" // TODO: make this fetched from mission process...
#define COMPUTE_MAX_DATA_SIZE (64 * 1024)          // matches payload_commander's MAX_PHOTO_SIZE ceiling
#define COMPUTE_COMPRESSED_CAP (COMPUTE_MAX_DATA_SIZE + 1024) // header + per-block overhead margin
#define COMPUTE_MAX_MSG_SIZE 256                    // matches obc_ipc's own MAX_IPC_PAYLOAD cap

static pthread_mutex_t job_lock = PTHREAD_MUTEX_INITIALIZER;
static int job_busy = 0;
static uint32_t job_id_running = 0;
static volatile int job_cancel_requested = 0; // not required to be volatile, but matches convention

static worker_job_t current_job; // One job at a time, safe to reuse
static int image_id_counter = 0;

static uint8_t input_buf[COMPUTE_MAX_DATA_SIZE];
static uint8_t compressed_buf[COMPUTE_COMPRESSED_CAP];

/* 
Recommend searching up 'Chunk Delay'. It makes the compression determinisitc and controlled
This is done by a delay
*/
static int chunk_delay_ms(void) {
    const char *env = getenv("COMPUTE_CHUNK_DELAY_MS");
    if (env == NULL) return 0;
    int val = atoi(env); // atoi turns string -> int
    return (val > 0) ? val : 0;
}

static int check_cancelled(uint32_t job_id) {
    int cancelled;
    pthread_mutex_lock(&job_lock);
    cancelled = job_busy && job_id_running == job_id && job_cancel_requested; // checking ID ensures previous jobs don't mess things up
    pthread_mutex_unlock(&job_lock);
    return cancelled;
}

void *worker_thread(void *arg) {
    worker_job_t *job = (worker_job_t *)arg;
    uint32_t job_id = job->req.job_id;
    OBC_Roles_t requester = job->requester;
    char in_path[COMPUTE_MAX_PATH];
    char out_path[COMPUTE_MAX_PATH];
    memcpy(&in_path, job->req.in_path, sizeof(in_path));
    memcpy(&out_path, job->req.out_path, sizeof(out_path));

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

        /* Cancellation Block */
        if (check_cancelled(job_id)) {
            compute_result_t result = { .job_id = job_id, .status = COMPUTE_STATUS_CANCELLED };
            IPC_send(requester, (const uint8_t *)&result, sizeof(result));
            pthread_mutex_lock(&job_lock); job_busy = 0;
            pthread_mutex_unlock(&job_lock);
            return NULL;
        }
        if (chunk_delay_ms() > 0) usleep((useconds_t)chunk_delay_ms() * 1000);
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

    /* Cancellation Block */
    if (check_cancelled(job_id)) {
            compute_result_t result = { .job_id = job_id, .status = COMPUTE_STATUS_CANCELLED };
            IPC_send(requester, (const uint8_t *)&result, sizeof(result));
            pthread_mutex_lock(&job_lock); job_busy = 0;
            pthread_mutex_unlock(&job_lock);
            return NULL;
        }
    if (chunk_delay_ms() > 0) usleep((useconds_t)chunk_delay_ms() * 1000);

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

        /* Cancellation Block */
        if (check_cancelled(job_id)) {
            compute_result_t result = { .job_id = job_id, .status = COMPUTE_STATUS_CANCELLED };
            IPC_send(requester, (const uint8_t *)&result, sizeof(result));
            pthread_mutex_lock(&job_lock); job_busy = 0;
            pthread_mutex_unlock(&job_lock);
            return NULL;
        }
        if (chunk_delay_ms() > 0) usleep((useconds_t)chunk_delay_ms() * 1000);
    }
    compute_result_t result = { .job_id = job_id, .status = COMPUTE_STATUS_OK, .output_size = (uint32_t)compressed_len };
    IPC_send(requester, (const uint8_t *)&result, sizeof(result));
    pthread_mutex_lock(&job_lock);
    job_busy = 0;
    pthread_mutex_unlock(&job_lock);
    return NULL;
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
    current_job.requester = src;
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
    compute_cancel_request_t req;
    memcpy(&req, buf, sizeof(req));

    pthread_mutex_lock(&job_lock);
    if (job_busy && job_id_running == req.job_id) {
        job_cancel_requested = 1;
        printf("[OBC COMPUTE] cancel requested for job %u\n", req.job_id);
    }
    pthread_mutex_unlock(&job_lock);
}