/* Exercise mode decisions with controlled fault bits, without starting tasks. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "manager/adcs_manager.h"
#include "manager/fault_manager.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "line %d: %s failed\n", __LINE__, #condition); \
        exit(1); \
    } \
} while (0)

#define TEST_NOW_US 1000000U

static uint32_t injected_faults;

uint32_t fault_management_get_active(void) {
    return injected_faults;
}

static void set_sensors(uint64_t timestamp_us, float measured_rate_rad_s) {
    adcs_sensor_packet_t sensors;

    memset(&sensors, 0, sizeof(sensors));
    sensors.timestamp_us = timestamp_us;
    sensors.valid_mask = ADCS_SENSOR_VALID_GYROSCOPE |
                         ADCS_SENSOR_VALID_MAGNETOMETER |
                         ADCS_SENSOR_VALID_SUN;
    sensors.angular_rate_rad_s[0] = measured_rate_rad_s;
    sensors.magnetic_field_t[2] = 30.0e-6F;
    sensors.sun_vector_body[0] = 1.0F;
    adcs_manager_set_sensors(&sensors);
}

static void set_attitude(uint64_t timestamp_us, uint8_t valid) {
    adcs_attitude_state_t attitude;

    memset(&attitude, 0, sizeof(attitude));
    attitude.timestamp_us = timestamp_us;
    attitude.quaternion[3] = 1.0F;
    attitude.confidence = 0.5F;
    attitude.valid = valid;
    adcs_manager_set_attitude(&attitude);
}

static void start_with_sun_reference(float measured_rate_rad_s,
                                     uint8_t attitude_valid) {
    adcs_reference_vectors_t references;

    injected_faults = ADCS_FAULT_NONE;
    adcs_manager_init();
    memset(&references, 0, sizeof(references));
    references.valid_mask = ADCS_REFERENCE_VALID_SUN;
    references.sun_eci_unit[0] = 1.0F;
    adcs_manager_set_references(NULL, &references);
    set_sensors(TEST_NOW_US, measured_rate_rad_s);
    set_attitude(TEST_NOW_US, attitude_valid);
}

static void test_attitude_fault_allows_detumble_but_blocks_pointing(void) {
    start_with_sun_reference(0.05F, 0U);
    injected_faults = ADCS_FAULT_ATTITUDE_INVALID;
    adcs_manager_update(TEST_NOW_US);
    CHECK(adcs_manager_get_mode() == ADCS_MODE_DETUMBLE);

    /* A settled measured rate and valid estimate are insufficient while the
       attitude fault bit is still active. */
    set_sensors(TEST_NOW_US, 0.0F);
    set_attitude(TEST_NOW_US, 1U);
    for (unsigned int cycle = 0U; cycle < 20U; ++cycle) {
        adcs_manager_update(TEST_NOW_US);
    }
    CHECK(adcs_manager_get_mode() == ADCS_MODE_DETUMBLE);

    injected_faults = ADCS_FAULT_NONE;
    adcs_manager_update(TEST_NOW_US);
    CHECK(adcs_manager_get_mode() == ADCS_MODE_SUN_ACQUISITION);

    injected_faults = ADCS_FAULT_ATTITUDE_INVALID;
    adcs_manager_update(TEST_NOW_US);
    CHECK(adcs_manager_get_mode() == ADCS_MODE_DETUMBLE);
}

static void test_other_faults_force_safe(void) {
    const uint32_t safe_faults[] = {
        ADCS_FAULT_SENSOR_STALE,
        ADCS_FAULT_SENSOR_RANGE,
        ADCS_FAULT_EXCESSIVE_RATE,
        ADCS_FAULT_ACTUATOR,
        ADCS_FAULT_TASK_DEADLINE
    };

    for (size_t index = 0U;
         index < sizeof(safe_faults) / sizeof(safe_faults[0]);
         ++index) {
        start_with_sun_reference(0.05F, 0U);
        adcs_manager_update(TEST_NOW_US);
        CHECK(adcs_manager_get_mode() == ADCS_MODE_DETUMBLE);
        injected_faults = safe_faults[index];
        adcs_manager_update(TEST_NOW_US);
        CHECK(adcs_manager_get_mode() == ADCS_MODE_SAFE);
    }
}

static void test_attitude_checks_work_before_fault_is_reported(void) {
    start_with_sun_reference(0.0F, 0U);
    adcs_manager_update(TEST_NOW_US);
    CHECK(adcs_manager_get_mode() == ADCS_MODE_SAFE);

    adcs_manager_request_mode(ADCS_MODE_DETUMBLE);
    adcs_manager_update(TEST_NOW_US);
    CHECK(adcs_manager_get_mode() == ADCS_MODE_DETUMBLE);

    start_with_sun_reference(0.0F, 1U);
    adcs_manager_update(TEST_NOW_US);
    CHECK(adcs_manager_get_mode() == ADCS_MODE_SUN_ACQUISITION);

    /* Sensor data remain fresh; only the attitude estimate expires. */
    set_sensors(TEST_NOW_US + 250001U, 0.0F);
    adcs_manager_update(TEST_NOW_US + 250001U);
    CHECK(adcs_manager_get_mode() == ADCS_MODE_DETUMBLE);
}

static void test_explicit_safe_request_takes_priority(void) {
    start_with_sun_reference(0.0F, 1U);
    adcs_manager_update(TEST_NOW_US);
    CHECK(adcs_manager_get_mode() == ADCS_MODE_SUN_ACQUISITION);

    injected_faults = ADCS_FAULT_ATTITUDE_INVALID;
    adcs_manager_request_mode(ADCS_MODE_SAFE);
    adcs_manager_update(TEST_NOW_US);
    CHECK(adcs_manager_get_mode() == ADCS_MODE_SAFE);
}

int main(void) {
    test_attitude_fault_allows_detumble_but_blocks_pointing();
    test_other_faults_force_safe();
    test_attitude_checks_work_before_fault_is_reported();
    test_explicit_safe_request_takes_priority();
    puts("ADCS manager mode-dependent fault tests passed");
    return 0;
}
