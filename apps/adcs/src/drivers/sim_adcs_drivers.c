/* Simulation implementations of the shared ADCS hardware contracts. */
#include "imu.h"
#include "magnetometer.h"
#include "magnetorquer.h"
#include "sun_sensor.h"

#include <string.h>

#include "control/control_math.h"
#include "simulation/adcs_simulator.h"

static uint8_t simulator_ready;
static magnetorquer_command_t last_magnetorquer_command;

static uint8_t simulator_is_ready(void) {
    adcs_simulator_truth_t truth;

    if (simulator_ready == 0U ||
        adcs_simulator_get_truth(&truth) != ADCS_RESULT_OK) {
        return 0U;
    }
    return 1U;
}

imu_status_t imu_initialize(void) {
    adcs_simulator_truth_t truth;

    if (adcs_simulator_get_truth(&truth) != ADCS_RESULT_OK) {
        simulator_ready = 0U;
        return IMU_ERROR;
    }
    simulator_ready = 1U;
    return IMU_OK;
}

imu_status_t imu_read(imu_sample_t *sample) {
    uint64_t timestamp_us;
    adcs_result_t result;

    if (sample == NULL) {
        return IMU_ERROR;
    }
    if (!simulator_is_ready()) {
        return IMU_NOT_READY;
    }
    result = adcs_simulator_read_imu(
        &timestamp_us,
        sample->angular_rate_rad_s,
        sample->acceleration_m_s2);
    if (result != ADCS_RESULT_OK) {
        return IMU_ERROR;
    }
    sample->timestamp_us = timestamp_us;
    sample->temperature_c = 25.0F;
    return IMU_OK;
}

magnetometer_status_t magnetometer_initialize(void) {
    adcs_simulator_truth_t truth;

    if (adcs_simulator_get_truth(&truth) != ADCS_RESULT_OK) {
        simulator_ready = 0U;
        return MAGNETOMETER_ERROR;
    }
    simulator_ready = 1U;
    return MAGNETOMETER_OK;
}

magnetometer_status_t magnetometer_read(magnetometer_sample_t *sample) {
    uint64_t timestamp_us;

    if (sample == NULL) {
        return MAGNETOMETER_ERROR;
    }
    if (!simulator_is_ready()) {
        return MAGNETOMETER_NOT_READY;
    }
    if (adcs_simulator_read_magnetometer(
            &timestamp_us,
            sample->magnetic_field_t) != ADCS_RESULT_OK) {
        return MAGNETOMETER_ERROR;
    }
    sample->timestamp_us = timestamp_us;
    return MAGNETOMETER_OK;
}

sun_sensor_status_t sun_sensor_initialize(void) {
    adcs_simulator_truth_t truth;

    if (adcs_simulator_get_truth(&truth) != ADCS_RESULT_OK) {
        simulator_ready = 0U;
        return SUN_SENSOR_ERROR;
    }
    simulator_ready = 1U;
    return SUN_SENSOR_OK;
}

sun_sensor_status_t sun_sensor_read(sun_sensor_sample_t *sample) {
    uint64_t timestamp_us;

    if (sample == NULL) {
        return SUN_SENSOR_ERROR;
    }
    if (!simulator_is_ready()) {
        return SUN_SENSOR_NOT_READY;
    }
    if (adcs_simulator_read_sun_sensor(
            &timestamp_us,
            sample->sun_vector_body,
            &sample->irradiance_w_m2) != ADCS_RESULT_OK) {
        return SUN_SENSOR_ERROR;
    }
    sample->timestamp_us = timestamp_us;
    sample->visible = 1U;
    return SUN_SENSOR_OK;
}

magnetorquer_status_t magnetorquer_initialize(void) {
    adcs_simulator_truth_t truth;

    if (adcs_simulator_get_truth(&truth) != ADCS_RESULT_OK) {
        simulator_ready = 0U;
        return MAGNETORQUER_ERROR;
    }
    simulator_ready = 1U;
    memset(&last_magnetorquer_command, 0, sizeof(last_magnetorquer_command));
    return MAGNETORQUER_OK;
}

magnetorquer_status_t magnetorquer_set(
    const magnetorquer_command_t *command) {
    adcs_magnetorquer_command_t simulator_command;

    if (command == NULL ||
        !adcs_values_are_finite(command->dipole_a_m2, 3U)) {
        return MAGNETORQUER_ERROR;
    }
    if (!simulator_is_ready()) {
        return MAGNETORQUER_NOT_READY;
    }

    memset(&simulator_command, 0, sizeof(simulator_command));
    simulator_command.timestamp_us = adcs_simulator_get_time_us();
    memcpy(
        simulator_command.dipole_a_m2,
        command->dipole_a_m2,
        sizeof(simulator_command.dipole_a_m2));
    simulator_command.enabled = command->enabled;
    if (adcs_simulator_set_magnetorquer(&simulator_command) != ADCS_RESULT_OK) {
        return MAGNETORQUER_ERROR;
    }
    last_magnetorquer_command = *command;
    return MAGNETORQUER_OK;
}

magnetorquer_status_t magnetorquer_disable(void) {
    magnetorquer_command_t command;

    memset(&command, 0, sizeof(command));
    return magnetorquer_set(&command);
}

magnetorquer_status_t magnetorquer_get_last_command(
    magnetorquer_command_t *command) {
    if (command == NULL) {
        return MAGNETORQUER_ERROR;
    }
    if (!simulator_is_ready()) {
        return MAGNETORQUER_NOT_READY;
    }
    *command = last_magnetorquer_command;
    return MAGNETORQUER_OK;
}
