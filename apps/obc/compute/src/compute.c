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
#include "rice_codec.h"

#define COMPUTE_MAX_DATA_SIZE (64 * 1024)          // matches payload_commander's MAX_PHOTO_SIZE ceiling
#define COMPUTE_COMPRESSED_CAP (COMPUTE_MAX_DATA_SIZE + 1024) // header + per-block overhead margin
#define COMPUTE_MAX_MSG_SIZE 256                    // matches obc_ipc's own MAX_IPC_PAYLOAD cap

/* Overridable so an integration test can widen the window to land a
   cancellation reliably -- a 64KB job over local IPC finishes in well
   under a millisecond, too fast to race a cancel against in practice.
   Production default is 0 (no delay) unless the env var is set. */
static int chunk_delay_ms(void)
{
    const char *env = getenv("COMPUTE_CHUNK_DELAY_MS");
    if (env == NULL) return 0;
    int val = atoi(env);
    return (val > 0) ? val : 0;
}

/* --- single job slot, shared between the dispatcher and worker threads --- */
static pthread_mutex_t job_lock = PTHREAD_MUTEX_INITIALIZER;
static int job_busy = 0;
static uint32_t job_id_running = 0;
static volatile int job_cancel_requested = 0;

typedef struct {
    compute_compress_request_t req;
    OBC_Roles_t requester;
} worker_job_t;

static worker_job_t current_job; // safe to reuse: only one job runs at a time

static uint8_t input_buf[COMPUTE_MAX_DATA_SIZE];
static uint8_t compressed_buf[COMPUTE_COMPRESSED_CAP];

/* --- mailbox: routes a reply from `data` to whichever worker is waiting on
   it. dispatch_thread is the only thread that ever calls IPC_receive --
   obc_ipc only supports one blocking receiver per process, so a second
   thread calling it directly would race the dispatcher's own accept(). --- */
static pthread_mutex_t reply_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t reply_cond = PTHREAD_COND_INITIALIZER;
static uint8_t reply_buf[COMPUTE_MAX_MSG_SIZE];
static int reply_len = 0;
static int reply_ready = 0;

/* Called only by dispatch_thread. This is a synchronous handoff, not a
   fire-and-forget write: if the worker hasn't consumed the previous reply
   yet, this blocks until it has, instead of overwriting the single slot
   and silently dropping a chunk. (A real bug caught by testing: without
   this wait, a fast run of data_read_reply_t chunks arriving back-to-back
   clobbered each other before the worker's memcpy loop could keep up,
   and only the last chunk before each drop survived.) */
static void deliver_reply(const uint8_t *buf, int len)
{
    pthread_mutex_lock(&reply_lock);
    while (reply_ready) {
        pthread_cond_wait(&reply_cond, &reply_lock);
    }
    memcpy(reply_buf, buf, (size_t)len);
    reply_len = len;
    reply_ready = 1;
    pthread_cond_signal(&reply_cond);
    pthread_mutex_unlock(&reply_lock);
}

/* Called only by the worker thread. */
static int wait_for_reply(uint8_t *buf, size_t buf_size)
{
    pthread_mutex_lock(&reply_lock);
    while (!reply_ready) {
        pthread_cond_wait(&reply_cond, &reply_lock);
    }
    int len = reply_len;
    if ((size_t)len <= buf_size) {
        memcpy(buf, reply_buf, (size_t)len);
    } else {
        len = -1;
    }
    reply_ready = 0;
    pthread_cond_signal(&reply_cond); // wake deliver_reply if it's blocked waiting for the slot to free up
    pthread_mutex_unlock(&reply_lock);
    return len;
}

static int check_cancelled(uint32_t job_id)
{
    int cancelled;
    pthread_mutex_lock(&job_lock);
    cancelled = job_busy && job_id_running == job_id && job_cancel_requested;
    pthread_mutex_unlock(&job_lock);
    return cancelled;
}

static void finish_job(uint32_t job_id, OBC_Roles_t requester, compute_status_t status, uint32_t output_size)
{
    compute_result_t result = { .job_id = job_id, .status = status, .output_size = output_size };
    IPC_send(requester, (const uint8_t *)&result, sizeof(result));

    pthread_mutex_lock(&job_lock);
    job_busy = 0;
    pthread_mutex_unlock(&job_lock);
}

static void *worker_thread(void *arg)
{
    worker_job_t *job = (worker_job_t *)arg;
    uint32_t job_id = job->req.job_id;
    OBC_Roles_t requester = job->requester;
    sample_width_t width = (sample_width_t)job->req.sample_width;
    char in_path[COMPUTE_MAX_PATH];
    char out_path[COMPUTE_MAX_PATH];
    memcpy(in_path, job->req.in_path, sizeof(in_path));
    memcpy(out_path, job->req.out_path, sizeof(out_path));
    int delay_ms = chunk_delay_ms();

    printf("[COMPUTE] job %u: reading %s\n", job_id, in_path);
    fflush(stdout);

    data_read_request_t read_req = {0};
    snprintf(read_req.path, sizeof(read_req.path), "%s", in_path);
    IPC_send(ROLE_DATA, (const uint8_t *)&read_req, sizeof(read_req));

    size_t input_len = 0;
    for (;;) {
        if (check_cancelled(job_id)) {
            printf("[COMPUTE] job %u cancelled during read\n", job_id);
            fflush(stdout);
            finish_job(job_id, requester, COMPUTE_STATUS_CANCELLED, 0);
            return NULL;
        }

        uint8_t buf[sizeof(data_read_reply_t)];
        int len = wait_for_reply(buf, sizeof(buf));
        if (len != sizeof(data_read_reply_t)) continue;

        data_read_reply_t reply;
        memcpy(&reply, buf, sizeof(reply));

        if (reply.status != 0) {
            printf("[COMPUTE] job %u: read of %s failed\n", job_id, in_path);
            fflush(stdout);
            finish_job(job_id, requester, COMPUTE_STATUS_FAILED, 0);
            return NULL;
        }

        if (input_len + reply.length > sizeof(input_buf)) {
            printf("[COMPUTE] job %u: input too large for input_buf\n", job_id);
            fflush(stdout);
            finish_job(job_id, requester, COMPUTE_STATUS_FAILED, 0);
            return NULL;
        }

        memcpy(input_buf + input_len, reply.payload, reply.length);
        input_len += reply.length;

        if (reply.is_last) break;
        if (delay_ms > 0) usleep((useconds_t)delay_ms * 1000);
    }

    printf("[COMPUTE] job %u: compressing %zu bytes (sample_width=%d)\n", job_id, input_len, (int)width);
    fflush(stdout);

    size_t compressed_len = 0;
    if (rice_compress(input_buf, input_len, width, compressed_buf, sizeof(compressed_buf), &compressed_len) != 0) {
        printf("[COMPUTE] job %u: compression failed\n", job_id);
        fflush(stdout);
        finish_job(job_id, requester, COMPUTE_STATUS_FAILED, 0);
        return NULL;
    }

    if (check_cancelled(job_id)) {
        printf("[COMPUTE] job %u cancelled after compression\n", job_id);
        fflush(stdout);
        finish_job(job_id, requester, COMPUTE_STATUS_CANCELLED, 0);
        return NULL;
    }

    printf("[COMPUTE] job %u: writing %zu compressed bytes to %s\n", job_id, compressed_len, out_path);
    fflush(stdout);

    size_t written = 0;
    while (written < compressed_len) {
        if (check_cancelled(job_id)) {
            printf("[COMPUTE] job %u cancelled during write\n", job_id);
            fflush(stdout);
            finish_job(job_id, requester, COMPUTE_STATUS_CANCELLED, 0);
            return NULL;
        }

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
        if (len != sizeof(data_write_ack_t)) {
            printf("[COMPUTE] job %u: malformed write ack\n", job_id);
            fflush(stdout);
            finish_job(job_id, requester, COMPUTE_STATUS_FAILED, 0);
            return NULL;
        }

        data_write_ack_t ack;
        memcpy(&ack, ack_buf, sizeof(ack));
        if (ack.status != 0) {
            printf("[COMPUTE] job %u: write to %s failed at offset %u\n", job_id, out_path, chunk.offset);
            fflush(stdout);
            finish_job(job_id, requester, COMPUTE_STATUS_FAILED, 0);
            return NULL;
        }

        written += chunk_len;
        if (delay_ms > 0 && written < compressed_len) usleep((useconds_t)delay_ms * 1000);
    }

    printf("[COMPUTE] job %u: done (%zu -> %zu bytes)\n", job_id, input_len, compressed_len);
    fflush(stdout);
    finish_job(job_id, requester, COMPUTE_STATUS_OK, (uint32_t)compressed_len);
    return NULL;
}

static void handle_compress_request(const uint8_t *buf, OBC_Roles_t src)
{
    compute_compress_request_t req;
    memcpy(&req, buf, sizeof(req));
    req.in_path[sizeof(req.in_path) - 1] = '\0';
    req.out_path[sizeof(req.out_path) - 1] = '\0';

    pthread_mutex_lock(&job_lock);
    if (job_busy) {
        pthread_mutex_unlock(&job_lock);
        printf("[COMPUTE] job %u rejected -- already running job %u\n", req.job_id, job_id_running);
        fflush(stdout);
        compute_result_t result = { .job_id = req.job_id, .status = COMPUTE_STATUS_BUSY };
        IPC_send(src, (const uint8_t *)&result, sizeof(result));
        return;
    }

    if (req.sample_width != 1 && req.sample_width != 2 && req.sample_width != 4) {
        pthread_mutex_unlock(&job_lock);
        printf("[COMPUTE] job %u rejected -- invalid sample_width %u\n", req.job_id, req.sample_width);
        fflush(stdout);
        compute_result_t result = { .job_id = req.job_id, .status = COMPUTE_STATUS_FAILED };
        IPC_send(src, (const uint8_t *)&result, sizeof(result));
        return;
    }

    job_busy = 1;
    job_id_running = req.job_id;
    job_cancel_requested = 0;
    pthread_mutex_unlock(&job_lock);

    current_job.req = req;
    current_job.requester = src;

    pthread_t worker;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    int ret = pthread_create(&worker, &attr, worker_thread, &current_job);
    pthread_attr_destroy(&attr);

    if (ret != 0) {
        printf("[COMPUTE] failed to start worker thread for job %u\n", req.job_id);
        fflush(stdout);
        pthread_mutex_lock(&job_lock);
        job_busy = 0;
        pthread_mutex_unlock(&job_lock);
        compute_result_t result = { .job_id = req.job_id, .status = COMPUTE_STATUS_FAILED };
        IPC_send(src, (const uint8_t *)&result, sizeof(result));
    }
}

static void handle_cancel_request(const uint8_t *buf)
{
    compute_cancel_request_t req;
    memcpy(&req, buf, sizeof(req));

    pthread_mutex_lock(&job_lock);
    if (job_busy && job_id_running == req.job_id) {
        job_cancel_requested = 1;
        printf("[COMPUTE] cancel requested for job %u\n", req.job_id);
        fflush(stdout);
    }
    pthread_mutex_unlock(&job_lock);
}

int dispatch_thread_init(void)
{
    printf("[COMPUTE] Attempting to create dispatch pthread.\n");
    pthread_t dispatch_pthread;
    int ret = pthread_create(&dispatch_pthread, NULL, dispatch_thread, NULL);
    if (ret != 0) {
        printf("[COMPUTE] Failed to create dispatch pthread.\n");
    } else {
        printf("[COMPUTE] Successfully created dispatch pthread.\n");
    }
    return ret;
}

void *dispatch_thread(void *arg)
{
    (void)arg;
    uint8_t buf[COMPUTE_MAX_MSG_SIZE];

    for (;;) {
        OBC_Roles_t src;
        int len = IPC_receive(&src, buf, sizeof(buf));
        if (len < 0) continue;

        if (src == ROLE_DATA) {
            /* A reply to something the worker asked `data` for
               (data_read_reply_t or data_write_ack_t). Routed by source
               role, not by size -- compute_cancel_request_t and
               data_write_ack_t happen to both be 4 bytes, so size alone
               can't tell them apart, but they can never come from the
               same role. */
            deliver_reply(buf, len);
        } else if (len == sizeof(compute_compress_request_t)) {
            handle_compress_request(buf, src);
        } else if (len == sizeof(compute_cancel_request_t)) {
            handle_cancel_request(buf);
        }
        /* anything else: not a message this process understands, drop it */
    }

    return NULL;
}
