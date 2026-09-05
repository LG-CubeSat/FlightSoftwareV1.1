#ifndef OBC_SLEEP_UNTIL_H
#define OBC_SLEEP_UNTIL_H

#include <time.h>

/* Sleeps until the absolute time `next`, without drifting from this
   function's own overhead. clock_nanosleep(TIMER_ABSTIME) does this
   natively on Linux (the real target); macOS's libc has no such call,
   so it's emulated there with a freshly-computed relative delta. */
static inline void obc_sleep_until(const struct timespec *next)
{
#if defined(__linux__)
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, next, NULL);
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    struct timespec delta;
    delta.tv_sec = next->tv_sec - now.tv_sec;
    delta.tv_nsec = next->tv_nsec - now.tv_nsec;
    if (delta.tv_nsec < 0) {
        delta.tv_sec -= 1;
        delta.tv_nsec += 1000000000L;
    }

    if (delta.tv_sec > 0 || (delta.tv_sec == 0 && delta.tv_nsec > 0)) {
        nanosleep(&delta, NULL);
    }
#endif
}

#endif // OBC_SLEEP_UNTIL_H
