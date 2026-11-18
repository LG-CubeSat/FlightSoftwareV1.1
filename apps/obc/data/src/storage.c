#include "storage.h"

#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include "telemetry_store.h"
#include "obc_ipc.h"
#include "obc_data_protocol.h"
#include "filesystem.h"
#include "data_health.h"
#include "csp_commands.h"
#include "obc_telemetry_protocol.h"

#define STORAGE_POLL_TIMEOUT_MS 1000
#define TELEMETRY_REPLY_RETRIES 20
#define TELEMETRY_REPLY_RETRY_DELAY_US 2000

// only storage thread reads or write this cache so it needs no mutex
static uint8_t latest_adcs_encoded[OBC_TELEMETRY_LATEST_MAX_ENCODED];
static uint16_t latest_adcs_length = 0;
static uint64_t latest_adcs_received_unix_us = 0;

static void send_latest_adcs(OBC_Roles_t requester)
{
    if (requester != ROLE_MISSION) return;

    obc_telemetry_latest_reply_t reply = {0};
    reply.magic = OBC_TELEMETRY_LATEST_REPLY_MAGIC;
    reply.length = latest_adcs_length;

    if (latest_adcs_length != 0) {
        memcpy(reply.encoded, latest_adcs_encoded, latest_adcs_length);
    }

    size_t wire_length = offsetof(obc_telemetry_latest_reply_t, encoded) + reply.length;

    if (IPC_send(requester, (const uint8_t *)&reply,
        (uint16_t)wire_length) < 0) {
        fprintf(stderr, "[STORAGE] could not send latest ADCS record.\n");
    }
}

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

static int send_telemetry_reply(
    OBC_Roles_t requester,
    const obc_telemetry_read_reply_t *reply
)
{
    for (
        int attempt = 0;
        attempt < TELEMETRY_REPLY_RETRIES;
        attempt++
    ) {
        if (
            IPC_send(
                requester,
                (const uint8_t *)reply,
                (uint16_t)sizeof(*reply)
            ) >= 0
        ) {
            return 0;
        }
        usleep(TELEMETRY_REPLY_RETRY_DELAY_US);
    }
    return -1;
}

static void send_telemetry_error(
    OBC_Roles_t requester,
    uint64_t requested_offset,
    obc_telemetry_read_status_t status
)
{
    obc_telemetry_read_reply_t reply = {
        .magic = OBC_TELEMETRY_READ_REPLY_MAGIC,
        .status = (int8_t)status,
        .is_last_chunk = 1,
        .end_of_log = status == OBC_TELEMETRY_READ_END,
        .record_offset = requested_offset,
        .next_offset = requested_offset
    };

    if (send_telemetry_reply(requester, &reply) != 0) {
        fprintf(
            stderr,
            "[STORAGE] failed to send telemetry read status to role %d\n",
            requester
        );
    }
}

static void handle_telemetry_read(
    OBC_Roles_t requester,
    const obc_telemetry_read_request_t *request
)
{
    if (requester != ROLE_MISSION) {
        fprintf(
            stderr,
            "[STORAGE] ignoring telemetry read from unauthorized role %d",
            requester
        );
        return;
    }

    uint8_t encoded[OBC_IPC_MAX_PAYLOAD];
    size_t record_size = 0;
    uint64_t next_offset = request->offset;
    int end_of_log = 0;

    obc_telemetry_read_status_t status = 
        telemetry_store_read_record(
            request->offset,
            encoded,
            sizeof(encoded),
            &record_size,
            &next_offset,
            &end_of_log
        );
    
    if (status != OBC_TELEMETRY_READ_OK) {
        send_telemetry_error(
            requester,
            request->offset,
            status
        );
        return;
    }

    size_t chunk_offset = 0;

    while(chunk_offset < record_size) {
        size_t remaining = record_size - chunk_offset;
        size_t chunk_length = remaining;

        if (chunk_length > OBC_TELEMETRY_READ_CHUNK_SIZE) {
            chunk_length = OBC_TELEMETRY_READ_CHUNK_SIZE;
        }

        int is_last_chunk = chunk_offset + chunk_length == record_size;

        obc_telemetry_read_reply_t reply = {
            .magic = OBC_TELEMETRY_READ_REPLY_MAGIC,
            .status = OBC_TELEMETRY_READ_OK,
            .is_last_chunk = (uint8_t)is_last_chunk,
            .end_of_log = (uint8_t)(is_last_chunk && end_of_log),
            .record_offset = request->offset,
            .next_offset = next_offset,
            .record_length = (uint16_t)record_size,
            .chunk_offset = (uint16_t)chunk_offset,
            .chunk_length = (uint16_t)chunk_length
        };

        memcpy(
            reply.payload,
            encoded + chunk_offset,
            chunk_length
        );

        if (send_telemetry_reply(requester, &reply) != 0) {
            fprintf(
                stderr,
                "[STORAGE] failed sending telemetry chunk at offset %zu\n", chunk_offset
            );
            return;
        }

        chunk_offset += chunk_length;
    }

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
            obc_telemetry_record_t record;
            if ((size_t)len <= sizeof(latest_adcs_encoded) &&
                obc_telemetry_decode(buf, (size_t)len, &record) == OBC_TELEMETRY_OK &&
                record.source_node == ADCS_ADDRESS &&
                record.source_port == ADCS_TELEM_PORT &&
                (latest_adcs_length == 0 ||
                 record.received_unix_us >= latest_adcs_received_unix_us)) {
                memcpy(latest_adcs_encoded, buf, (size_t)len);
                latest_adcs_length = (uint16_t)len;
                latest_adcs_received_unix_us = record.received_unix_us;
            }

            if (telemetry_store_append(buf, (size_t)len) != 0) {
                fprintf(
                    stderr,
                    "[STORAGE] failed to persist telemetry record\n"
                );
            }
        } else if (len == (int)sizeof(obc_telemetry_latest_request_t)) {
            obc_telemetry_latest_request_t request;
            memcpy(&request, buf, sizeof(request));

            if (request.magic == OBC_TELEMETRY_LATEST_REQUEST_MAGIC) {
                send_latest_adcs(src);
            }
        } else if (len == (int)sizeof(obc_telemetry_read_request_t)) {
            obc_telemetry_read_request_t request;
            memcpy(&request, buf, sizeof(request));

            if (request.magic == OBC_TELEMETRY_READ_REQUEST_MAGIC) {
                handle_telemetry_read(src, &request);
            } else {
                fprintf(stderr,
                        "[STORAGE] ignoring unknown %d-byte request from role %d\n",
                        len, src);
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
