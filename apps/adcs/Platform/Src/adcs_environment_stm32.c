#include "platform/adcs_environment.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stm32g4xx_hal.h"
#include <string.h>

static uint32_t previous_ms;
static uint64_t elapsed_ms;
static uint64_t synchronized_unix_us;
static uint64_t synchronized_boot_us;

void adcs_environment_step(float dt_s) { (void)dt_s; }
uint64_t adcs_monotonic_time_us(void) {
    uint64_t result;
    taskENTER_CRITICAL();
    uint32_t now = HAL_GetTick();
    elapsed_ms += (uint32_t)(now - previous_ms);
    previous_ms = now;
    result = elapsed_ms * 1000ULL;
    taskEXIT_CRITICAL();
    return result;
}
uint64_t adcs_unix_time_us(void) {
    uint64_t result;
    taskENTER_CRITICAL();
    uint64_t now = adcs_monotonic_time_us();
    result = synchronized_unix_us == 0U ? 0U :
        synchronized_unix_us + (now - synchronized_boot_us);
    taskEXIT_CRITICAL();
    return result;
}
adcs_result_t adcs_set_unix_time_us(uint64_t time) {
    if (time == 0U || time > UINT64_MAX - UINT64_C(315576000000000))
        return ADCS_RESULT_INVALID_ARGUMENT;
    taskENTER_CRITICAL();
    synchronized_boot_us = adcs_monotonic_time_us();
    synchronized_unix_us = time;
    taskEXIT_CRITICAL();
    return ADCS_RESULT_OK;
}
adcs_result_t adcs_get_orbit(adcs_orbit_state_t *orbit) {
    if (orbit == NULL) return ADCS_RESULT_INVALID_ARGUMENT;
    memset(orbit, 0, sizeof(*orbit));
    return ADCS_RESULT_UNAVAILABLE; /* No flight orbit provider is configured. */
}
adcs_result_t adcs_inject_simulator_faults(uint32_t mask) {
    (void)mask;
    return ADCS_RESULT_UNAVAILABLE;
}
