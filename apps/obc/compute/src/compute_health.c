#include "compute_health.h"

#include "obc_progress.h"
#include <errno.h>
#include <stdlib.h>

/*
Dispatch will eventually wake once per second, so three seconds gives
it room for scheudling delay without hiding a real stall for too long
*/
#define DISPATCH_STALL_TIMEOUT_MS 3000

/*
A worker can legititmately wait up to five seconds for a data reply.
Give it additonal headroom before declaring it stuck.
TODO: This should be tuned eventually
*/
#define WORKER_STALL_TIMEOUT_MS 10000
#define WORKER_STALL_TIMEOUT_ENV "COMPUTE_WORKER_STALL_TIMEOUT_MS"

static obc_progress_t dispatch_watch;
static obc_progress_t worker_watch;

static uint64_t timeout_from_env(
    const char *name,
    uint64_t fallback
) {
    const char *text = getenv(name);

    if (text == NULL || text[0] == '\0') {
        return fallback;
    }

    errno = 0;
    char *end = NULL;

    unsigned long long parsed = strtoull(text, &end, 10); // strtoull = str to unsigned long long

    if (errno != 0 ||
        end == text ||
        *end != '\0' ||
        parsed == 0) {
        return fallback;
    }
    return (uint64_t)parsed;
}

int compute_health_init(void)
{
    uint64_t worker_timeout_ms = timeout_from_env(
        WORKER_STALL_TIMEOUT_ENV,
        WORKER_STALL_TIMEOUT_MS
    );

    int ret = obc_progress_init(
        &dispatch_watch,
        DISPATCH_STALL_TIMEOUT_MS
    );

    if (ret != 0) {
        return ret;
    }

    ret = obc_progress_init(
        &worker_watch,
        worker_timeout_ms
    );

    if (ret != 0) {
        obc_progress_destroy(&dispatch_watch);
        return ret;
    }

    /*
    Dispatch is expected to run for the entire process lifetime.
    Worker remains idle until a compression request is accepted.
    */
   obc_progress_begin(&dispatch_watch);

   return 0;
}

void compute_health_dispatch_progress(void)
{
    obc_progress_touch(&dispatch_watch);
}

void compute_health_worker_begin(void)
{
    obc_progress_begin(&worker_watch);
}

void compute_health_worker_progress(void) {
    obc_progress_touch(&worker_watch);
}

void compute_health_worker_end(void)
{
    obc_progress_end(&worker_watch);
}

int compute_health_is_healthy(void)
{
    return obc_progress_is_healthy(&dispatch_watch) && obc_progress_is_healthy(&worker_watch);
}
