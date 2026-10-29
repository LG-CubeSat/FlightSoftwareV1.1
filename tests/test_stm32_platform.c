/* Exercise the real STM32 time and unavailable-device adapters on the host.
 * Only the HAL tick and critical-section primitives are replaced. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform/adcs_environment.h"
#include "imu.h"
#include "magnetometer.h"
#include "sun_sensor.h"
#include "magnetorquer.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static uint32_t tick;
static int critical_depth;
uint32_t HAL_GetTick(void) { return tick; }
void test_enter_critical(void) { ++critical_depth; }
void test_exit_critical(void) { CHECK(critical_depth > 0); --critical_depth; }

int main(void) {
    CHECK(adcs_unix_time_us() == 0);
    tick = 1234;
    CHECK(adcs_monotonic_time_us() == UINT64_C(1234000));
    adcs_environment_step(900.0F);
    CHECK(adcs_monotonic_time_us() == UINT64_C(1234000));
    CHECK(adcs_set_unix_time_us(0) == ADCS_RESULT_INVALID_ARGUMENT);
    CHECK(adcs_set_unix_time_us(UINT64_MAX) == ADCS_RESULT_INVALID_ARGUMENT);
    const uint64_t epoch = UINT64_C(1700000000000000);
    CHECK(adcs_set_unix_time_us(epoch) == ADCS_RESULT_OK);
    tick += 2000;
    CHECK(adcs_unix_time_us() == epoch + 2000000);
    uint64_t monotonic = adcs_monotonic_time_us();
    CHECK(adcs_set_unix_time_us(epoch - 1000000) == ADCS_RESULT_OK);
    CHECK(adcs_monotonic_time_us() == monotonic);
    tick = UINT32_MAX - 2;
    monotonic = adcs_monotonic_time_us();
    tick = 4;
    CHECK(adcs_monotonic_time_us() == monotonic + 7000);
    CHECK(critical_depth == 0);

    adcs_orbit_state_t orbit;
    memset(&orbit, 0xA5, sizeof(orbit));
    CHECK(adcs_get_orbit(&orbit) == ADCS_RESULT_UNAVAILABLE);
    const unsigned char *bytes = (const unsigned char *)&orbit;
    for (size_t i = 0; i < sizeof(orbit); ++i) CHECK(bytes[i] == 0);
    CHECK(adcs_get_orbit(NULL) == ADCS_RESULT_INVALID_ARGUMENT);
    CHECK(adcs_inject_simulator_faults(0xFFFFFFFFU) == ADCS_RESULT_UNAVAILABLE);
    imu_sample_t imu;
    magnetometer_sample_t mag;
    sun_sensor_sample_t sun;
    memset(&sun, 0xA5, sizeof(sun));
    CHECK(imu_initialize() == IMU_NOT_READY && imu_read(&imu) == IMU_NOT_READY);
    CHECK(magnetometer_initialize() == MAGNETOMETER_NOT_READY && magnetometer_read(&mag) == MAGNETOMETER_NOT_READY);
    CHECK(sun_sensor_initialize() == SUN_SENSOR_NOT_READY && sun_sensor_read(&sun) == SUN_SENSOR_NOT_READY);
    CHECK(imu.timestamp_us == 0 && mag.timestamp_us == 0 && sun.visible == 0);
    magnetorquer_command_t command = {.enabled = 1, .dipole_a_m2 = {0.1F, 0.2F, 0.3F}};
    CHECK(magnetorquer_initialize() == MAGNETORQUER_NOT_READY);
    CHECK(magnetorquer_set(&command) == MAGNETORQUER_NOT_READY);
    CHECK(magnetorquer_disable() == MAGNETORQUER_NOT_READY);
    CHECK(magnetorquer_get_last_command(&command) == MAGNETORQUER_NOT_READY);
    CHECK(command.enabled == 0 && command.dipole_a_m2[0] == 0);
    puts("STM32 clock rollover, synchronization, and unavailable hardware checks passed");
    return 0;
}
