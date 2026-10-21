#include "telemetry_store.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "obc_telemetry_protocol.h"

#ifndef DEFAULT_TELEMETRY_LOG_PATH
#define DEFAULT_TELEMETRY_LOG_PATH "/tmp/obc_telemetry.bin"
#endif
#define TELEMETRY_LOG_PATH_ENV "OBC_TELEMETRY_LOG_PATH"

static const char *telemetry_log_path(void)
{
    const char *configured = getenv(TELEMETRY_LOG_PATH_ENV);

    if (configured != NULL && configured[0] != '\0') {
        return configured;
    }

    return DEFAULT_TELEMETRY_LOG_PATH;
}

int telemetry_store_append(
    const uint8_t *encoded,
    size_t encoded_size
)
{
    /*
    Decode before writing so malformed or trucated records never enter log
    */
    obc_telemetry_record_t record;

    obc_telemetry_status_t status = obc_telemetry_decode(
        encoded,
        encoded_size,
        &record
    );

    if (status != OBC_TELEMETRY_OK) {
        fprintf(
            stderr,
            "[TELEMETRY STORE] rejected invalid record: %d\n",
            status
        );
        return -1;
    }

    const char *path = telemetry_log_path();
    FILE *file = fopen(path, "ab");

    if (file == NULL) {
        perror("[TELEMETRY STORE] fopen");
        return -1;
    }

    size_t written = fwrite(
        encoded,
        1,
        encoded_size,
        file
    );

    /*
    fclose() also flushes the stdio buffer. Its result matters because
    delayed write errors can be reported during that flush
    */
    int close_result = fclose(file);

    if (written != encoded_size || close_result != 0) {
        fprintf(
            stderr,
            "[TELEMETRY STORE] failed to append complete record to %s\n",
            path
        );
        return -1;
    }

    return 0;
}

obc_telemetry_read_status_t telemetry_store_read_record(
    uint64_t offset,
    uint8_t *encoded_out,
    size_t encoded_capacity,
    size_t *record_size_out,
    uint64_t *next_offset_out,
    int *end_of_log_out
)
{
    if (encoded_out == NULL || record_size_out == NULL ||
        next_offset_out == NULL || end_of_log_out == NULL) {
        return OBC_TELEMETRY_READ_INVALID_ARGUMENT;
    }

    *record_size_out = 0;
    *next_offset_out = offset;
    *end_of_log_out = 0;

    if (encoded_capacity < OBC_IPC_MAX_PAYLOAD ||
        offset > (uint64_t)INT64_MAX) {
        return OBC_TELEMETRY_READ_INVALID_ARGUMENT;
    }

    FILE *file = fopen(telemetry_log_path(), "rb");
    if (file == NULL) {
        if (errno == ENOENT) {
            *end_of_log_out = 1;
            return OBC_TELEMETRY_READ_END;
        }
        perror("[TELEMETRY STORE] fopen for read");
        return OBC_TELEMETRY_READ_IO_ERROR;
    }

    if (fseeko(file, (off_t)offset, SEEK_SET) != 0) {
        fclose(file);
        return OBC_TELEMETRY_READ_IO_ERROR;
    }

    uint8_t header[OBC_TELEMETRY_HEADER_SIZE];
    size_t header_bytes = fread(header, 1, sizeof(header), file);

    if (header_bytes == 0 && feof(file)) {
        fclose(file);
        *end_of_log_out = 1;
        return OBC_TELEMETRY_READ_END;
    }
    if (header_bytes != sizeof(header)) {
        fclose(file);
        return OBC_TELEMETRY_READ_INVALID_RECORD;
    }

    uint16_t payload_length =
        ((uint16_t)header[16] << 8U) | (uint16_t)header[17];
    size_t record_size = OBC_TELEMETRY_HEADER_SIZE + payload_length;

    if (payload_length > OBC_TELEMETRY_MAX_PAYLOAD ||
        record_size > encoded_capacity) {
        fclose(file);
        return OBC_TELEMETRY_READ_INVALID_RECORD;
    }

    memcpy(encoded_out, header, sizeof(header));
    size_t payload_bytes = fread(
        encoded_out + OBC_TELEMETRY_HEADER_SIZE,
        1,
        payload_length,
        file
    );
    if (payload_bytes != payload_length) {
        fclose(file);
        return OBC_TELEMETRY_READ_INVALID_RECORD;
    }

    obc_telemetry_record_t decoded;
    if (obc_telemetry_decode(encoded_out, record_size, &decoded) !=
        OBC_TELEMETRY_OK) {
        fclose(file);
        return OBC_TELEMETRY_READ_INVALID_RECORD;
    }

    if (offset > UINT64_MAX - record_size) {
        fclose(file);
        return OBC_TELEMETRY_READ_INVALID_RECORD;
    }

    *record_size_out = record_size;
    *next_offset_out = offset + record_size;

    int next_byte = fgetc(file);
    if (next_byte == EOF) {
        if (ferror(file)) {
            fclose(file);
            return OBC_TELEMETRY_READ_IO_ERROR;
        }
        *end_of_log_out = 1;
    }

    fclose(file);
    return OBC_TELEMETRY_READ_OK;
}
