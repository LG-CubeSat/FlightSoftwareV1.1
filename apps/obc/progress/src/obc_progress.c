#include "obc_progress.h"

#include <stddef.h>

static void monotonic_now(struct timespec *out)
{
    clock_gettime(CLOCK_MONOTONIC, out);
}

static uint64_t elapsed_ms(const struct timespec *start, const struct timespec *end)
{
    int64_t seconds = (int64_t)end->tv_sec - (int64_t)start->tv_sec;
    int64_t nanoseconds = (int64_t)end->tv_nsec - (int64_t)start->tv_nsec;

    int64_t total_nanoseconds = seconds * 1000000000LL + nanoseconds;

    if (total_nanoseconds <= 0) {
        return 0;
    }

    return (uint64_t)(total_nanoseconds / 1000000LL);
}

int obc_progress_init(obc_progress_t *watch, uint64_t timeout_ms)
{
    if (watch == NULL || timeout_ms == 0) {
        return -1;
    }

    int ret = pthread_mutex_init(&watch->lock, NULL);
    if (ret != 0) {
        return ret;
    }

    monotonic_now(&watch->last_progress);
    watch->timeout_ms = timeout_ms;
    watch->active= 0;

    return 0;
}

void obc_progress_begin(obc_progress_t *watch)
{
    struct timespec now;
    monotonic_now(&now);

    pthread_mutex_lock(&watch->lock);
    watch->active = 1;
    watch->last_progress= now;
    pthread_mutex_unlock(&watch->lock);
}

void obc_progress_touch(obc_progress_t *watch)
{
    struct timespec now;
    monotonic_now(&now);

    pthread_mutex_lock(&watch->lock);

    if (watch->active) {
        watch->last_progress = now;
    }

    pthread_mutex_unlock(&watch->lock);
}

void obc_progress_end(obc_progress_t *watch)
{
    pthread_mutex_lock(&watch->lock);
    watch->active = 0;
    pthread_mutex_unlock(&watch->lock);
}

int obc_progress_is_healthy(obc_progress_t *watch)
{
    struct timespec now;
    struct timespec last_progress;
    uint64_t timeout_ms;
    int active;
    
    monotonic_now(&now);

    /*
    copy the state while holding the lock, then perform the calculation
    after releasing it. this keeps the critical section short.
    */
    pthread_mutex_lock(&watch->lock);
    active = watch->active;
    last_progress = watch->last_progress;
    timeout_ms = watch->timeout_ms;
    pthread_mutex_unlock(&watch->lock);

    if (!active) {
        return 1;
    }

    return elapsed_ms(&last_progress, &now) <= timeout_ms;
}

void obc_progress_destroy(obc_progress_t *watch)
{
    pthread_mutex_destroy(&watch->lock);
}