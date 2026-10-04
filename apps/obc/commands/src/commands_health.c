#include "commands_health.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include "obc_progress.h"

#define COMMANDS_STALL_TIMEOUT_MS 3000
#define INGEST_STALL_TIMEOUT_ENV "COMMANDS_INGEST_STALL_TIMEOUT_MS"
#define RELAY_STALL_TIMEOUT_ENV "COMMANDS_RELAY_STALL_TIMEOUT_MS"

static obc_progress_t ingest_watch;
static obc_progress_t relay_watch;

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

int commands_health_init(void)
{
    int ret = obc_progress_init(
        &ingest_watch,
        timeout_from_env(INGEST_STALL_TIMEOUT_ENV, COMMANDS_STALL_TIMEOUT_MS)
    );
    if (ret != 0) {
        return ret;
    }

    ret = obc_progress_init(
        &relay_watch,
        timeout_from_env(RELAY_STALL_TIMEOUT_ENV, COMMANDS_STALL_TIMEOUT_MS)
    );
    if (ret != 0) {
        obc_progress_destroy(&ingest_watch);
        return ret;
    }

    obc_progress_begin(&ingest_watch);
    obc_progress_begin(&relay_watch);
    return 0;
}

void commands_health_ingest_progress(void)
{
    obc_progress_touch(&ingest_watch);
}

void commands_health_relay_progress(void)
{
    obc_progress_touch(&relay_watch);
}

int commands_health_is_healthy(void)
{
    return obc_progress_is_healthy(&ingest_watch) &&
        obc_progress_is_healthy(&relay_watch);
}
