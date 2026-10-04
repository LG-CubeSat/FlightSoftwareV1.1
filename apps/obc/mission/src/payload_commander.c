#include "payload_commander.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "csp_commands.h"
#include "camera.h"
#include "radio.h"
#include "obc_ipc.h"
#include "obc_relay_protocol.h"
#include "obc_data_protocol.h"
#include "obc_compute_protocol.h"
#include "time.h"
#include "mission_health.h"
#include "obc_telemetry_protocol.h"

#define MAX_PHOTO_SIZE (64 * 1024) // 64kb, tune to real photo size

/* 
Compute's own job deadline is 60s (COMPUTE_JOB_TIMEOUT_MS), 
so give it headroom before we conclude the whole process is gone.
*/
#define COMPRESS_TOTAL_TIMEOUT_MS 90000
#define DOWNLINK_CHUNK_TIMEOUT_MS 10000
#define DOWNLINK_TOTAL_TIMEOUT_MS 120000
#define TELEMETRY_READ_TIMEOUT_MS 10000

static uint8_t photo_buf[MAX_PHOTO_SIZE];

static void deadline_in_ms(struct timespec *out, int ms)
{
    clock_gettime(CLOCK_MONOTONIC, out);
    out->tv_sec += ms / 1000;
    out->tv_nsec += (long)(ms % 1000) * 1000000L;
    if (out->tv_nsec >= 1000000000L) { out->tv_sec += 1; out->tv_nsec -= 1000000000L; }
}

static int remaining_ms(const struct timespec *deadline)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long ms = (long)(deadline->tv_sec - now.tv_sec) * 1000 + (deadline->tv_nsec - now.tv_nsec) / 1000000L;
    return (ms > 0) ? (int)ms: 0;
}

int payload_commander_take_photo(const char *out_path)
{
    printf("[PAYLOAD COMMANDER] Requesting photo capture\n");
    fflush(stdout);
    if (camera_capture(out_path) != 0) {
       fprintf(stderr, "[PAYLOAD COMMANDER] photo capture failed\n");
        return -1;
    }
    mission_health_payload_progress();
    return 0;
}

int payload_commander_compress_photo(const char *in_path, const char *out_path)
{
    printf("[PAYLOAD COMMANDER] Requesting compression of %s\n", in_path);
    fflush(stdout);

    static uint32_t next_job_id = 1;
    uint32_t job_id = next_job_id++;

    compute_compress_request_t req = {0};
    req.job_id = job_id;
    snprintf(req.in_path, sizeof(req.in_path), "%s", in_path);
    snprintf(req.out_path, sizeof(req.out_path), "%s", out_path);
    req.sample_width = 1; // the mock photo is a raw byte stream, not fixed-width samples

    IPC_send(ROLE_COMPUTE, (const uint8_t *)&req, sizeof(req));

    struct timespec deadline;
    deadline_in_ms(&deadline, COMPRESS_TOTAL_TIMEOUT_MS);

    for (;;) {
        OBC_Roles_t src;
        uint8_t buf[sizeof(compute_result_t)];
        int len = IPC_receive_timeout(&src, buf, sizeof(buf), remaining_ms(&deadline));
        
        if (len == IPC_TIMEOUT) {
            fprintf(stderr, "[PAYLOAD COMMANDER] no result for compute for %s in %d ms\n", in_path, COMPRESS_TOTAL_TIMEOUT_MS);
            return -1;
        }
        if (len != (int)sizeof(compute_result_t)) continue;

        compute_result_t result;
        memcpy(&result, buf, sizeof(result));
        if (result.job_id != job_id) continue; // stale reply from an earlier job, not ours

        mission_health_payload_progress();

        if (result.status != COMPUTE_STATUS_OK) {
            fprintf(stderr, "[PAYLOAD COMMANDER] compression of %s failed (status=%d)\n", in_path, result.status);
            return -1;
        }

        printf("[PAYLOAD COMMANDER] Compression done: %u bytes\n", result.output_size);
        fflush(stdout);
        return 0;
    }
}

int payload_commander_downlink_photo(const char *photo_path)
{
    printf("[PAYLOAD COMMANDER] Downlinking Photo\n");
    fflush(stdout);

    data_read_request_t req = {0};
    snprintf(req.path, sizeof(req.path), "%s", photo_path);
    IPC_send(ROLE_DATA, (const uint8_t *)&req, sizeof(req));

    size_t total = 0;
    
    struct timespec total_deadline, chunk_deadline;
    deadline_in_ms(&total_deadline, DOWNLINK_TOTAL_TIMEOUT_MS);

    for (;;) {
        deadline_in_ms(&chunk_deadline, DOWNLINK_CHUNK_TIMEOUT_MS);

        int wait = remaining_ms(&chunk_deadline);
        int total_left = remaining_ms(&total_deadline);
        if (total_left < wait) wait = total_left;

        OBC_Roles_t src;
        uint8_t buf[sizeof(data_read_reply_t)];
        int len = IPC_receive_timeout(&src, buf, sizeof(buf), wait);

        if (len == IPC_TIMEOUT) {
            fprintf(stderr, "[PAYLOAD COMMANDER] data went quit while reading%s\n", photo_path);
            return -1;
        }
        if (len != sizeof(data_read_reply_t)) continue;

        data_read_reply_t reply;
        memcpy(&reply, buf, sizeof(reply));

        if (reply.status != 0) {
            fprintf(stderr, "[PAYLOAD COMMANDER] data reported an error reading %s\n", photo_path);
            return -1;
        }

        if (total + reply.length > sizeof(photo_buf)) {
            fprintf(stderr, "[PAYLOAD COMMANDER] photo too large for photo_buf\n");
            return -1;
        }

        memcpy(photo_buf + total, reply.payload, reply.length);
        total += reply.length;
        mission_health_payload_progress();

        if (reply.is_last) break;
    }

    if (total == 0) {
        fprintf(stderr, "[PAYLOAD COMMANDER] no data read from %s\n", photo_path);
        return -1;
    }

    // sending it
    if (radio_send(photo_buf, total) != 0) {
        fprintf(stderr, "[PAYLOAD COMMANDER] downlink failed.\n");
        return -1;
    }

    mission_health_payload_progress();

    return 0;
}

int payload_commander_point_to_sun(void)
{
    command_envelope_t cmd = { .command_id = CMD_POINT_TO_SUN, .seq = 1 };
    relay_request_t req = { .dest_addr = ADCS_ADDRESS, .dest_port = ADCS_CMD_PORT, .length = sizeof(cmd) };
    memcpy(req.payload, &cmd, sizeof(cmd));
    return IPC_send(ROLE_COMMANDS, (const uint8_t *)&req, sizeof(req)) < 0 ? -1 : 0;
}

int payload_commander_downlink_telemetry_record(
    uint64_t offset,
    uint64_t *next_offset,
    int *end_of_log
) {
    if (
        next_offset == NULL ||
        end_of_log == NULL
    ) {
        return -1;
    }
    // do not modifty the caller's cursor until the entire record has been received and successfully passed to radio
    obc_telemetry_read_request_t request = {
        .magic=OBC_TELEMETRY_READ_REQUEST_MAGIC,
        .offset = offset
    };

    if (IPC_send(
        ROLE_DATA, 
        (const uint8_t *)&request, 
        (uint16_t)sizeof(request)
        ) < 0
    ) {
        fprintf(stderr,
            "[PAYLOD COMMANDER] failed requesting telemetry at offset %llu\n",
            (unsigned long long)offset
        );
        return -1;
    }

    uint8_t encoded[OBC_IPC_MAX_PAYLOAD];
    size_t encoded_size = 0;

    uint16_t expected_record_length = 0;
    uint64_t expected_next_offset = offset;
    int expected_end_of_log = 0;
    int received_first_chunk = 0;

    struct timespec deadline;
    deadline_in_ms(&deadline, TELEMETRY_READ_TIMEOUT_MS);

    for (;;) {
        int wait_ms = remaining_ms(&deadline);
        if (wait_ms == 0) {
            fprintf(stderr,
                "[PAYLOAD COMMANDER] telemetry request timed out\n"
            );
            return -1;
        }

        OBC_Roles_t source;
        obc_telemetry_read_reply_t reply;

        int length = IPC_receive_timeout(
            &source,
            (uint8_t *)&reply,
            sizeof(reply),
            wait_ms
        );

        if (length == IPC_TIMEOUT) {
            fprintf(stderr,
                "[PAYLOAD COMMANDER] telemetry request timeout\n");
            return -1;
        }

        /*
        Mission may receive other IPC messages. They are not replies to this request, so ignore them.
        */
        if (
            length != (int)sizeof(reply) ||
            source != ROLE_DATA ||
            reply.magic != OBC_TELEMETRY_READ_REPLY_MAGIC
        ) {
            continue;
        }

        /*
        END is not an error. it means the cursor points to the current end of telem log
        */
        if (reply.status == OBC_TELEMETRY_READ_END) {
            if (
                !reply.is_last_chunk ||
                !reply.end_of_log ||
                reply.chunk_length != 0 ||
                reply.record_offset != offset ||
                reply.next_offset != offset
            ) {
                fprintf(stderr,
                    "[PAYLOAD COMMANDER] malformed telemetry END reply"
                );
                return -1;
            }

            *next_offset = reply.next_offset;
            *end_of_log = 1;
            return 0;
        }

        if (reply.status != OBC_TELEMETRY_READ_OK) {
            fprintf(stderr,
                "[PAYLOAD COMMANDER] Data returned telemetry status %d\n",
                reply.status
            );
            return -1;
        }

        /*
        These checks prevent malformed or inconsistent chunks from
        overflowing the destination buffer
        */
        if (
            reply.record_offset != offset ||
            reply.record_length == 0 ||
            reply.record_length > sizeof(encoded) ||
            reply.chunk_length > sizeof(reply.payload) ||
            reply.chunk_offset != encoded_size ||
            (size_t)reply.chunk_offset + reply.chunk_length > reply.record_length
        ) {
            fprintf(stderr,
                "[PAYLOAD COMMANDER] malformed telemetry chunk\n"
            );
            return -1;
        }

        if (!received_first_chunk) {
            expected_record_length = reply.record_length;
            expected_next_offset = reply.next_offset;
            received_first_chunk = 1;
        } else if (reply.record_length != expected_record_length || reply.next_offset != expected_next_offset) {
            fprintf(stderr,
                "[PAYLOAD COMMANDER] inconsistent telemetry chunks\n"
            );
            return -1;
        }

        memcpy(
            encoded + reply.chunk_offset,
            reply.payload,
            reply.chunk_length
        );
        encoded_size += reply.chunk_length;

        if (!reply.is_last_chunk) {
            if (reply.end_of_log) {
                fprintf(
                    stderr,
                    "[PAYLOAD COMMANDER] non-final chunk marked end-of-log\n"
                );
                return -1;
            }
            continue;
        }

        /*
        Data attaches end_of_log to the final chunk because only that chunk completes the record
        */
        expected_end_of_log = reply.end_of_log;
        
        if (encoded_size != expected_record_length) {
            fprintf(stderr,
                "[PAYLOAD COMMANDER] incomplete telemetry record\n");
            return -1;
        }
        
        /*
        Decode once more before transmission. Data already validated the record. 
        But this ensures Mission never downlinks malformed bytes
        If the IPC reply is corruped
        */
        obc_telemetry_record_t decoded;
        if (
            obc_telemetry_decode(
                encoded,
                encoded_size,
                &decoded
            ) != OBC_TELEMETRY_OK
        ) {
            fprintf(stderr,
                "[PAYLOAD COMMANDER] invalid telemetry record\n"
            );
            return -1;
        }

        if (radio_send(encoded, encoded_size) != 0) {
            fprintf(stderr,
                "[PAYLOAD COMMANDER] telemetry downlink failed\n"
            );
            return -1;
        }

        /*
        Commit the new cursor only after radio_send succeeds. On failure the caller
        can retry using its unchanged old cursor.
        */
        *next_offset = expected_next_offset;
        *end_of_log = expected_end_of_log;

        mission_health_payload_progress();

        printf("[PAYLOAD COMMANDER] downlinked telemetry: %zu bytes, next offset %llu\n", encoded_size, (unsigned long long)*next_offset);
        fflush(stdout);

        return 0;
    }
}

int payload_commander_downlink_telemetry_batch(
    uint64_t *cursor,
    size_t max_records,
    size_t *records_sent,
    int *end_of_log
) {
    if (
        cursor == NULL ||
        records_sent == NULL ||
        end_of_log == NULL ||
        max_records == 0
    ) {
        return -1;
    }

    *records_sent = 0;
    *end_of_log = 0;

    for (size_t i = 0; i < max_records; i++) {
        /*
        Work with a candidate cursor. 
        The caller's cursor is only updated after one complete record reaches the radio.
        */
        uint64_t next_cursor = *cursor;
        int record_reached_end = 0;

        int result = payload_commander_downlink_telemetry_record(
            *cursor,
            &next_cursor,
            &record_reached_end
        );

        if (result != 0) {
            return -1;
        }

        /*
        At the empty tail, Data returns READ_END and leaves the cursor unchanged.
        No record was transmitted
        */
       if (next_cursor == *cursor) {
            if (!record_reached_end) {
                fprintf(
                    stderr,
                    "[PAYLOAD COMMANDER] telemetry read made no progress\n"
                );
                return -1;
            }

            *end_of_log = 1;
            return 0;
        }
        
        /*
        The single record function successfully transmitted the record.
        Use the returned cursor for next.
        */
        *cursor = next_cursor;
        (*records_sent)++;

        if (record_reached_end) {
            *end_of_log = 1;
            return 0;
        }
    }

    /*
    reaching here is success. stopped because record allow is complete.
    nothing failed
    */
    return 0;
}