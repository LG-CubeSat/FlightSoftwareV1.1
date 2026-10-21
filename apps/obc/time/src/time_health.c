#include "time_health.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include "obc_progress.h"

#define TIME_STALL_TIMEOUT_MS 3000
#define BROADCAST_STALL_TIMEOUT_ENV "TIME_BROADCAST_STALL_TIMEOUT_MS"
#define REQUEST_STALL_TIMEOUT_ENV "TIME_REQUEST_STALL_TIMEOUT_MS"

static obc_progress_t broadcast_watch;
static obc_progress_t request_watch;

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

int time_health_init(void)
{
    int ret = obc_progress_init(
        &broadcast_watch,
        timeout_from_env(BROADCAST_STALL_TIMEOUT_ENV, TIME_STALL_TIMEOUT_MS)
    );
    if (ret != 0) {
        return ret;
    }

    ret = obc_progress_init(
        &request_watch,
        timeout_from_env(REQUEST_STALL_TIMEOUT_ENV, TIME_STALL_TIMEOUT_MS)
    );
    if (ret != 0) {
        obc_progress_destroy(&broadcast_watch);
        return ret;
    }

    /* Broadcast is idle between intervals; the listener runs continuously. */
    obc_progress_begin(&request_watch);
    return 0;
}

void time_health_broadcast_begin(void)
{
    obc_progress_begin(&broadcast_watch);
}

void time_health_broadcast_progress(void)
{
    obc_progress_touch(&broadcast_watch);
}

void time_health_broadcast_end(void)
{
    obc_progress_end(&broadcast_watch);
}

void time_health_request_progress(void)
{
    obc_progress_touch(&request_watch);
}

int time_health_is_healthy(void)
{
    return obc_progress_is_healthy(&broadcast_watch) &&
        obc_progress_is_healthy(&request_watch);
}
