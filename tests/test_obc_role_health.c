#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "commands_health.h"
#include "data_health.h"
#include "mission_health.h"
#include "time_health.h"

#define TEST_TIMEOUT_MS "80"
#define STALE_WAIT_MS 120

static int total_checks = 0;
static int failed_checks = 0;

static void check(const char *label, int condition)
{
    total_checks++;
    printf("[%s] %s\n", condition ? "PASS" : "FAIL", label);
    if (!condition) {
        failed_checks++;
    }
}

static void sleep_ms(long milliseconds)
{
    struct timespec delay = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (milliseconds % 1000) * 1000000L,
    };
    nanosleep(&delay, NULL);
}

static void configure_short_timeouts(void)
{
    setenv("DATA_STORAGE_STALL_TIMEOUT_MS", TEST_TIMEOUT_MS, 1);
    setenv("COMMANDS_INGEST_STALL_TIMEOUT_MS", TEST_TIMEOUT_MS, 1);
    setenv("COMMANDS_RELAY_STALL_TIMEOUT_MS", TEST_TIMEOUT_MS, 1);
    setenv("MISSION_SCHEDULER_STALL_TIMEOUT_MS", TEST_TIMEOUT_MS, 1);
    setenv("MISSION_PAYLOAD_STALL_TIMEOUT_MS", TEST_TIMEOUT_MS, 1);
    setenv("MISSION_AUTONOMY_STALL_TIMEOUT_MS", TEST_TIMEOUT_MS, 1);
    setenv("TIME_BROADCAST_STALL_TIMEOUT_MS", TEST_TIMEOUT_MS, 1);
    setenv("TIME_REQUEST_STALL_TIMEOUT_MS", TEST_TIMEOUT_MS, 1);
}

int main(void)
{
    configure_short_timeouts();

    check("data health initializes", data_health_init() == 0);
    check("data begins healthy", data_health_is_healthy());
    sleep_ms(STALE_WAIT_MS);
    check("data detects stalled storage", !data_health_is_healthy());
    data_health_storage_progress();
    check("data recovers after storage progress", data_health_is_healthy());

    check("commands health initializes", commands_health_init() == 0);
    check("commands begins healthy", commands_health_is_healthy());
    sleep_ms(STALE_WAIT_MS);
    check("commands detects a stalled worker", !commands_health_is_healthy());
    commands_health_ingest_progress();
    check(
        "commands remains unhealthy while relay is stale",
        !commands_health_is_healthy()
    );
    commands_health_relay_progress();
    check("commands recovers when both workers progress", commands_health_is_healthy());

    check("mission health initializes", mission_health_init() == 0);
    check("mission begins healthy", mission_health_is_healthy());
    sleep_ms(STALE_WAIT_MS);
    check("mission detects a stalled worker", !mission_health_is_healthy());
    mission_health_scheduler_progress();
    check(
        "mission remains unhealthy while autonomy is stale",
        !mission_health_is_healthy()
    );
    mission_health_autonomy_progress();
    check("mission recovers when both workers progress", mission_health_is_healthy());

    mission_health_payload_begin();
    sleep_ms(STALE_WAIT_MS);
    mission_health_autonomy_progress();
    check("mission detects stalled active payload work", !mission_health_is_healthy());
    mission_health_payload_progress();
    check("mission recovers after payload progress", mission_health_is_healthy());
    mission_health_payload_end();
    check("mission returns to healthy scheduler work", mission_health_is_healthy());

    check("time health initializes", time_health_init() == 0);
    check("time begins healthy with broadcast idle", time_health_is_healthy());
    sleep_ms(STALE_WAIT_MS);
    check("time detects a stalled request listener", !time_health_is_healthy());
    time_health_request_progress();
    check("time request-listener progress recovers health", time_health_is_healthy());

    time_health_broadcast_begin();
    sleep_ms(STALE_WAIT_MS);
    time_health_request_progress();
    check("time detects a stalled active broadcast", !time_health_is_healthy());
    time_health_broadcast_progress();
    check("time recovers after broadcast progress", time_health_is_healthy());
    time_health_broadcast_end();
    check("an idle broadcast remains healthy", time_health_is_healthy());

    if (failed_checks == 0) {
        printf(
            "obc_role_health_test: PASS (%d/%d checks)\n",
            total_checks,
            total_checks
        );
        return 0;
    }

    fprintf(
        stderr,
        "obc_role_health_test: FAIL (%d/%d checks failed)\n",
        failed_checks,
        total_checks
    );
    return 1;
}
