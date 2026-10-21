#include "data_health.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include "obc_progress.h"

#define STORAGE_STALL_TIMEOUT_MS 3000
#define STORAGE_STALL_TIMEOUT_ENV "DATA_STORAGE_STALL_TIMEOUT_MS"

static obc_progress_t storage_watch;

static uint64_t timeout_from_env(const char *name, uint64_t fallback)
{
    const char *text = getenv(name);
    if (text == NULL || text[0] == '\0') {
        return fallback;
    }

    errno = 0;
    char *end = NULL;
    unsigned long long parsed = strtoull(text, &end, 10);

    if (errno != 0 || end == text || *end != '\0' || parsed == 0) {
        return fallback;
    }

    return (uint64_t)parsed;
}

int data_health_init(void)
{
    int ret = obc_progress_init(
        &storage_watch,
        timeout_from_env(STORAGE_STALL_TIMEOUT_ENV, STORAGE_STALL_TIMEOUT_MS)
    );
    if (ret == 0) {
        obc_progress_begin(&storage_watch);
    }
    return ret;
}

void data_health_storage_progress(void)
{
    obc_progress_touch(&storage_watch);
}

int data_health_is_healthy(void)
{
    return obc_progress_is_healthy(&storage_watch);
}
