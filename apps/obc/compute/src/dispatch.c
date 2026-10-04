#include "dispatch.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>

#include "worker.h"
#include "obc_ipc.h"
#include "obc_compute_protocol.h"
#include "obc_data_protocol.h"
#include "ssdv_codec.h"
#include "compute_health.h"

#define CALL_SIGN "COM" // TODO: make this fetched from mission process...
#define COMPUTE_MAX_DATA_SIZE (64 * 1024)          // matches payload_commander's MAX_PHOTO_SIZE ceiling
#define COMPUTE_COMPRESSED_CAP (COMPUTE_MAX_DATA_SIZE + 1024) // header + per-block overhead margin
#define COMPUTE_MAX_MSG_SIZE 256                    // matches obc_ipc's own MAX_IPC_PAYLOAD cap

# define REPLY_SLOT_GRACE_MS 250

#define DISPATCH_POLL_TIMEOUT_MS 1000

static pthread_mutex_t reply_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t reply_cond = PTHREAD_COND_INITIALIZER;
static uint8_t reply_buf[COMPUTE_MAX_MSG_SIZE];
static int reply_len = 0;
static int reply_ready = 0;
static uint32_t reply_epoch = 0; // bumper once per job
static int epoch_open = 0; // is a worker currently consuming?

static void grace_deadline(struct timespec *out, int ms) {
    clock_gettime(CLOCK_REALTIME, out);
    out->tv_sec += ms / 1000;
    out->tv_nsec += (long)(ms % 1000) * 1000000L;
    if (out->tv_nsec >= 1000000000L) {
        out->tv_sec += 1; out->tv_nsec -= 1000000000L;        
    }
}

uint32_t dispatch_job_begin(void) {
    pthread_mutex_lock(&reply_lock);
    reply_epoch++;
    epoch_open = 1;
    reply_ready = 0; // discard any previous job leftovers
    uint32_t epoch = reply_epoch;
    pthread_cond_broadcast(&reply_cond);
    pthread_mutex_unlock(&reply_lock);
    return epoch;
}

void dispatch_job_end(uint32_t e) {
    pthread_mutex_lock(&reply_lock);
    if (reply_epoch == e && epoch_open) {
        epoch_open = 0;
        reply_ready = 0;
        pthread_cond_broadcast(&reply_cond); // release deliver reply if its holding
    }
    pthread_mutex_unlock(&reply_lock);
}

/* Called only by dispatch_thread, when a message arrives from ROLE_DATA. */
static void deliver_reply(const uint8_t *buf, int len) {
    if (len < 0 || (size_t)len > sizeof(reply_buf)) return;

    struct timespec deadline;
    grace_deadline(&deadline, REPLY_SLOT_GRACE_MS);

    pthread_mutex_lock(&reply_lock);
    uint32_t entry_epoch = reply_epoch;

    // wait for the worker to drain the slot. Bounded and abandoned if the job ends while we wait.
    while (reply_ready && epoch_open && reply_epoch == entry_epoch) {
        if (pthread_cond_timedwait(&reply_cond, &reply_lock, &deadline) == ETIMEDOUT) break;
    }
    
    if (!epoch_open || reply_epoch != entry_epoch || reply_ready) {
        pthread_mutex_unlock(&reply_lock);
        printf("[OBC COMPUTE dropped a %d byte reply from data (nobody waiting for it)\n", len);
        fflush(stdout);
        return;
    }

    memcpy(reply_buf, buf, (size_t)len);
    reply_len = len;
    reply_ready = 1;
    pthread_cond_signal(&reply_cond); // wake the worker
    pthread_mutex_unlock(&reply_lock);
}

int wait_for_reply(uint8_t *buf, size_t buf_size, uint32_t epoch, const struct timespec *abs_deadline) {
    pthread_mutex_lock(&reply_lock);
    while (!reply_ready) {
        if (!epoch_open || reply_epoch != epoch) {
            pthread_mutex_unlock(&reply_lock);
            return WAIT_REPLY_ABORTED;
        }

        int rc = pthread_cond_timedwait(&reply_cond, &reply_lock, abs_deadline); // sleep until deliver_reply signals
        
        if (rc == ETIMEDOUT && !reply_ready) {
            // the otherside isn't there. They didn't reply by abs_deadline
            // so we have to break to avoid infinite loop
            pthread_mutex_unlock(&reply_lock);
            return WAIT_REPLY_TIMEOUT; // specific failure mode for timeout
        }
        
    }

    int len = reply_len;
    if ((size_t)len <= buf_size) {
        memcpy(buf, reply_buf, (size_t)len);
    } else {
        len = WAIT_REPLY_TOO_BIG; // failure mode for reply being too long for buffer
    }

    reply_ready = 0;
    pthread_cond_signal(&reply_cond); // wake deliver_reply if it's waiting for the slot to free up
    pthread_mutex_unlock(&reply_lock);
    return len;
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

void *dispatch_thread(void *arg) {
    (void)arg;
    uint8_t buf[COMPUTE_MAX_MSG_SIZE];

    for (;;) {
        OBC_Roles_t src;
        int len = IPC_receive_timeout(&src, buf, sizeof(buf), DISPATCH_POLL_TIMEOUT_MS);

        /*
        An ordinary timoeut proves the dispatch thread wokeup and completed aonther polling interval
        */
        if (len == IPC_TIMEOUT) {
            compute_health_dispatch_progress();
            continue;
        }
        
        /* 
        don't count receive errors as progress. if error continue
        indefinitely, the dispatch watch will eventually become stale.
        */
        if (len < 0) continue;

        printf("[OBC COMPUTE] got %d bytes from role %d\n", len, src);

        if (src == ROLE_DATA) {
            deliver_reply(buf, len);
        } else if (len == sizeof(compute_compress_request_t)) {
            handle_compress_request(buf, src);
        } else if (len == sizeof(compute_cancel_request_t)) {
            handle_cancel_request(buf);
        }

        /* 
        record progress after processing. not immediately after receiving.
        if a handler wedges, this line is never reached.
        */
        compute_health_dispatch_progress();
    }
}