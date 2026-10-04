#include "mission_health.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include "obc_progress.h"

#define SCHEDULER_STALL_TIMEOUT_MS 3000
/* Compute may legitimately take up to 90 seconds to return a result. */
#define PAYLOAD_STALL_TIMEOUT_MS 100000
#define AUTONOMY_STALL_TIMEOUT_MS 3000
#define SCHEDULER_STALL_TIMEOUT_ENV "MISSION_SCHEDULER_STALL_TIMEOUT_MS"
#define PAYLOAD_STALL_TIMEOUT_ENV "MISSION_PAYLOAD_STALL_TIMEOUT_MS"
#define AUTONOMY_STALL_TIMEOUT_ENV "MISSION_AUTONOMY_STALL_TIMEOUT_MS"

static obc_progress_t scheduler_watch;
static obc_progress_t payload_watch;
static obc_progress_t autonomy_watch;

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

int mission_health_init(void)
{
    int ret = obc_progress_init(
        &scheduler_watch,
        timeout_from_env(
            SCHEDULER_STALL_TIMEOUT_ENV,
            SCHEDULER_STALL_TIMEOUT_MS
        )
    );
    if (ret != 0) {
        return ret;
    }

    ret = obc_progress_init(
        &payload_watch,
        timeout_from_env(PAYLOAD_STALL_TIMEOUT_ENV, PAYLOAD_STALL_TIMEOUT_MS)
    );
    if (ret != 0) {
        obc_progress_destroy(&scheduler_watch);
        return ret;
    }

    ret = obc_progress_init(
        &autonomy_watch,
        timeout_from_env(AUTONOMY_STALL_TIMEOUT_ENV, AUTONOMY_STALL_TIMEOUT_MS)
    );
    if (ret != 0) {
        obc_progress_destroy(&scheduler_watch);
        obc_progress_destroy(&payload_watch);
        return ret;
    }

    obc_progress_begin(&scheduler_watch);
    obc_progress_begin(&autonomy_watch);
    return 0;
}

void mission_health_scheduler_progress(void)
{
    obc_progress_touch(&scheduler_watch);
}

void mission_health_payload_begin(void)
{
    /* The scheduler is synchronously executing payload work, so the payload
     * watch temporarily owns responsibility for that thread's progress. */
    obc_progress_end(&scheduler_watch);
    obc_progress_begin(&payload_watch);
}

void mission_health_payload_progress(void)
{
    obc_progress_touch(&payload_watch);
}

void mission_health_payload_end(void)
{
    obc_progress_end(&payload_watch);
    obc_progress_begin(&scheduler_watch);
}

void mission_health_autonomy_progress(void)
{
    obc_progress_touch(&autonomy_watch);
}

int mission_health_is_healthy(void)
{
    return obc_progress_is_healthy(&scheduler_watch) &&
        obc_progress_is_healthy(&payload_watch) &&
        obc_progress_is_healthy(&autonomy_watch);
}
