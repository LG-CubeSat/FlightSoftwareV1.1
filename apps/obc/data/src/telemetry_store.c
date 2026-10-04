#include "telemetry_store.h"

#include <stdio.h>
#include <stdlib.h>

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