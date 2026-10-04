#include "storage.h"

#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>

#include "telemetry_store.h"
#include "obc_ipc.h"
#include "obc_data_protocol.h"
#include "filesystem.h"
#include "data_health.h"

#define STORAGE_POLL_TIMEOUT_MS 1000

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

    /* sized to the larger of the two request types this thread handles */
    uint8_t buf[OBC_IPC_MAX_PAYLOAD];

    for (;;) {
        OBC_Roles_t src;
        int len = IPC_receive_timeout(
            &src,
            buf,
            sizeof(buf),
            STORAGE_POLL_TIMEOUT_MS
        );

        if (len == IPC_TIMEOUT) {
            data_health_storage_progress();
            continue;
        }
        if (len < 0) {
            continue;
        }

        if (src == ROLE_COMMANDS) {
            if (telemetry_store_append(buf, (size_t)len) != 0) {
                fprintf(
                    stderr,
                    "[STORAGE] failed to persist telemetry record\n"
                );
            }
        } else if (len == sizeof(data_read_request_t)) {
            data_read_request_t req;
            memcpy(&req, buf, sizeof(req));
            req.path[sizeof(req.path) - 1] = '\0';  // don't trust the sender to have NUL terminated it.

            printf("[STORAGE] Streaming %s to role %d\n", req.path, src);
            fflush(stdout);

            filesystem_stream_file(req.path, src);
        } else if (len == sizeof(data_write_chunk_t)) {
            data_write_chunk_t chunk;
            memcpy(&chunk, buf, sizeof(chunk));
            chunk.path[sizeof(chunk.path) - 1] = '\0';

            printf("[STORAGE] Writing %u bytes to %s (offset=%u, last=%d) for role %d\n",
                   chunk.length, chunk.path, chunk.offset, chunk.is_last, src);
            fflush(stdout);

            filesystem_write_chunk(chunk.path, chunk.offset, chunk.length, chunk.payload, src);
        }
        /* anything else: not a message this thread understands, drop it */
        data_health_storage_progress();
    }

    return NULL;
}
