#include "compute_health.h"

#include "obc_progress.h"

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

static obc_progress_t dispatch_watch;
static obc_progress_t worker_watch;

int compute_health_init(void)
{
    int ret = obc_progress_init(
        &dispatch_watch,
        DISPATCH_STALL_TIMEOUT_MS
    );

    if (ret != 0) {
        return ret;
    }

    ret = obc_progress_init(
        &worker_watch,
        WORKER_STALL_TIMEOUT_MS
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