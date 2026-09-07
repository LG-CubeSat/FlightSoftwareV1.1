#include "storage.h"

#include <stdio.h>
#include <stdint.h>
#include <pthread.h>

#include "obc_ipc.h"
#include "obc_data_protocol.h"
#include "filesystem.h"

int storage_thread_init(void) {
    printf("[DATA STORAGE] Attempting to create pthread.\n");
    
    pthread_t storage_pthread;
    int ret = pthread_create(&storage_pthread, NULL, storage_thread, NULL);
    if (ret != 0) {
        printf("[DATA STORAGE] Failed to create pthread.\n");
    } else {
        printf("[DATA STORAGE] Successfully created pthread.\n");
    }
    return ret;
}

void *storage_thread(void *arg)
{
    (void)arg;

    for (;;) {
        OBC_Roles_t src;
        uint8_t buf[sizeof(data_read_request_t)];
        int len = IPC_recieve(&src, buf, sizeof(buf));

        data_read_request_t req;
        memcpy(&req, buf, sizeof(req));
        req.path[sizeof(req.path) - 1] = '\0';  // don't trust the sender to have NUL terminated it.

        printf("[STORAGE] Streaming %s to role %d\n", req.path, src);
        fflush(stdout);

        filesystem_stream_file(req.path, src);
    }

    return NULL;
}
