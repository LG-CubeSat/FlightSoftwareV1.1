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

void *dispatch_thread(void *arg) {
    (void)arg;
    uint8_t buf[COMPUTE_MAX_MSG_SIZE];

    for (;;) {
        OBC_Roles_t src;
        int len = IPC_receive(&src, buf, sizeof(buf));

        if (len < 0) continue;

        printf("[OBC COMPUTE] got %d bytes from role %d\n", len, src);
    }
}

int compression_thread_init(void) {
    printf("[OBC COMPUTE] Attempting to create compression pthread.\n");
    pthread_t compression_pthread;
    int ret = pthread_create(&compression_pthread, NULL, compression_thread, NULL);
    if (ret != 0) {
        printf("[OBC COMPUTE] Failed to create compression pthread.\n");
    } else {
        printf("[OBC COMPUTE] Successfully created compression pthread.\n");
    }
    return ret;
}

void *compression_thread(void *arg) {
    (void)arg;

    
}