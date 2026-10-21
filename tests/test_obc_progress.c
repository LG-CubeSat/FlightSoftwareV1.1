#include <stdio.h>
#include <time.h>

#include "obc_progress.h"

#define TEST_TIMEOUT_MS 50

static void sleep_ms(long milliseconds)
{
    struct timespec delay = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (milliseconds % 1000) * 1000000L,
    };

    nanosleep(&delay, NULL);
}

static int expect(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "[FAIL] %s\n", message);
        return 1;
    }

    printf("[PASS] %s\n", message);
    return 0;
}

int main(void)
{
    obc_progress_t watch;
    int failures = 0;

    failures += expect(
        obc_progress_init(&watch, TEST_TIMEOUT_MS) == 0,
        "watch initializes"
    );

    failures += expect(
        obc_progress_is_healthy(&watch),
        "idle watch is healthy"
    );

    obc_progress_begin(&watch);

    failures += expect(
        obc_progress_is_healthy(&watch),
        "newly active watch is healthy"
    );

    sleep_ms(30);
    obc_progress_touch(&watch);
    sleep_ms(30);

    failures += expect(
        obc_progress_is_healthy(&watch),
        "touch extends the progress deadline"
    );

    sleep_ms(60);

    failures += expect(
        !obc_progress_is_healthy(&watch),
        "active watch becomes unhealthy after its timeout"
    );

    obc_progress_end(&watch);

    failures += expect(
        obc_progress_is_healthy(&watch),
        "ending work returns the watch to healthy idle"
    );

    obc_progress_destroy(&watch);

    if (failures != 0) {
        fprintf(stderr,
        "obc_progress_test: FAIL (%d checks failed)\n",
        failures);
        return 1;
    }

    printf("obc_progress_test: PASS\n");
    return 0;
}
