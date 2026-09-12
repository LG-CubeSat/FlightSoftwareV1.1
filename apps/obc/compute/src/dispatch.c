#include "dispatch.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "worker.h"
#include "obc_ipc.h"
#include "obc_compute_protocol.h"
#include "obc_data_protocol.h"
#include "ssdv_codec.h"

#define CALL_SIGN "COM" // TODO: make this fetched from mission process...
#define COMPUTE_MAX_DATA_SIZE (64 * 1024)          // matches payload_commander's MAX_PHOTO_SIZE ceiling
#define COMPUTE_COMPRESSED_CAP (COMPUTE_MAX_DATA_SIZE + 1024) // header + per-block overhead margin
#define COMPUTE_MAX_MSG_SIZE 256                    // matches obc_ipc's own MAX_IPC_PAYLOAD cap

static pthread_mutex_t reply_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t reply_cond = PTHREAD_COND_INITIALIZER;
static uint8_t reply_buf[COMPUTE_MAX_MSG_SIZE];
static int reply_len = 0;
static int reply_ready = 0;

/* Called only by dispatch_thread, when a message arrives from ROLE_DATA. */
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

int wait_for_reply(uint8_t *buf, size_t buf_size) {
    pthread_mutex_lock(&reply_lock);
    while (!reply_ready) {
        pthread_cond_wait(&reply_cond, &reply_lock); // sleep until deliver_reply signals
    }
    int len = reply_len;
    if ((size_t)len <= buf_size) {
        memcpy(buf, reply_buf, (size_t)len);
    } else {
        len = -1;
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