#include "simulation/adcs_simulator.h"

#include <math.h>
#include <pthread.h>
#include <string.h>

#include "control/control_math.h"
#include "estimation/reference_vectors.h"

#define SIM_EARTH_RADIUS_M 6371000.0F
#define SIM_EARTH_MU_M3_S2 3.986004418e14
#define SIM_DEFAULT_UNIX_TIME_US 1704067200000000ULL
#define SIM_PI_F 3.14159265358979323846F
#define SIM_RATE_LIMIT_RAD_S 2.0F
#define SIM_VISCOUS_DAMPING_NM_S 2.0e-7F

typedef struct {
    adcs_simulator_config_t config;
    adcs_simulator_truth_t truth;
    uint32_t faults;
    uint8_t initialized;
} simulator_state_t;

static simulator_state_t simulator;
static pthread_mutex_t simulator_lock = PTHREAD_MUTEX_INITIALIZER;

static void quaternion_multiply(const versor left, const versor right, versor output) {
    versor value;

    value[0] = left[3] * right[0] + left[0] * right[3] +
               left[1] * right[2] - left[2] * right[1];
    value[1] = left[3] * right[1] - left[0] * right[2] +
               left[1] * right[3] + left[2] * right[0];
    value[2] = left[3] * right[2] + left[0] * right[1] -
               left[1] * right[0] + left[2] * right[3];
    value[3] = left[3] * right[3] - left[0] * right[0] -
               left[1] * right[1] - left[2] * right[2];
    memcpy(output, value, sizeof(versor));
}

static void quaternion_rotate_vector(
    const versor quaternion,
    const float vector[ADCS_VECTOR_LENGTH],
    float output[ADCS_VECTOR_LENGTH]) {
    float quaternion_vector[ADCS_VECTOR_LENGTH] = {
        quaternion[0], quaternion[1], quaternion[2]
    };
    float first_cross[ADCS_VECTOR_LENGTH];
    float second_cross[ADCS_VECTOR_LENGTH];

    adcs_vector_cross(quaternion_vector, vector, first_cross);
    for (size_t axis = 0U; axis < ADCS_VECTOR_LENGTH; ++axis) {
        first_cross[axis] *= 2.0F;
    }
    adcs_vector_cross(quaternion_vector, first_cross, second_cross);
    for (size_t axis = 0U; axis < ADCS_VECTOR_LENGTH; ++axis) {
        output[axis] = vector[axis] +
                       quaternion[3] * first_cross[axis] +
                       second_cross[axis];
    }
}

static adcs_simulator_config_t default_config(void) {
    adcs_simulator_config_t config;

    memset(&config, 0, sizeof(config));
    config.initial_quaternion[3] = 1.0F;
    config.initial_angular_rate_rad_s[0] = 0.08F;
    config.initial_angular_rate_rad_s[1] = -0.06F;
    config.initial_angular_rate_rad_s[2] = 0.045F;
    config.inertia_kg_m2[0] = 0.0050F;
    config.inertia_kg_m2[1] = 0.0060F;
    config.inertia_kg_m2[2] = 0.0045F;
    config.gyro_bias_rad_s[0] = 0.0003F;
    config.gyro_bias_rad_s[1] = -0.0002F;
    config.gyro_bias_rad_s[2] = 0.0001F;
    config.maximum_dipole_a_m2 = 0.20F;
    config.maximum_reaction_wheel_torque_nm = 0.00002F;
    config.maximum_reaction_wheel_momentum_nms = 0.005F;
    config.orbit_altitude_m = 500000.0F;
    config.orbit_inclination_rad = 51.6F * SIM_PI_F / 180.0F;
    config.initial_unix_time_us = SIM_DEFAULT_UNIX_TIME_US;
    return config;
}

static uint8_t config_is_valid(const adcs_simulator_config_t *config) {
    versor normalized;

    if (config == NULL ||
        adcs_quaternion_normalize(
            config->initial_quaternion,
            normalized) != ADCS_RESULT_OK ||
        !adcs_values_are_finite(
            config->initial_angular_rate_rad_s,
            ADCS_VECTOR_LENGTH) ||
        !adcs_values_are_finite(config->inertia_kg_m2, ADCS_VECTOR_LENGTH) ||
        !adcs_values_are_finite(config->gyro_bias_rad_s, ADCS_VECTOR_LENGTH) ||
        !isfinite(config->maximum_dipole_a_m2) ||
        !isfinite(config->maximum_reaction_wheel_torque_nm) ||
        !isfinite(config->maximum_reaction_wheel_momentum_nms) ||
        !isfinite(config->orbit_altitude_m) ||
        !isfinite(config->orbit_inclination_rad) ||
        config->maximum_dipole_a_m2 <= 0.0F ||
        config->maximum_reaction_wheel_torque_nm <= 0.0F ||
        config->maximum_reaction_wheel_momentum_nms <= 0.0F ||
        config->orbit_altitude_m <= 100000.0F ||
        config->initial_unix_time_us == 0U) {
        return 0U;
    }

    for (size_t axis = 0U; axis < ADCS_VECTOR_LENGTH; ++axis) {
        if (config->inertia_kg_m2[axis] <= 0.0F) {
            return 0U;
        }
    }
    return 1U;
}

static void update_environment_locked(void) {
    float radius = SIM_EARTH_RADIUS_M + simulator.config.orbit_altitude_m;
    double mean_motion = sqrt(SIM_EARTH_MU_M3_S2 /
                              ((double)radius * radius * radius));
    double elapsed_s = (double)simulator.truth.timestamp_us / 1000000.0;
    double phase = mean_motion * elapsed_s;
    float inclination = simulator.config.orbit_inclination_rad;
    float sun_eci[ADCS_VECTOR_LENGTH] = {1.0F, 0.0F, 0.0F};
    float field_eci[ADCS_VECTOR_LENGTH] = {0.0F, 0.0F, 0.0F};

    simulator.truth.orbit.unix_time_us = simulator.truth.unix_time_us;
    simulator.truth.orbit.position_eci_m[0] = radius * (float)cos(phase);
    simulator.truth.orbit.position_eci_m[1] =
        radius * (float)sin(phase) * cosf(inclination);
    simulator.truth.orbit.position_eci_m[2] =
        radius * (float)sin(phase) * sinf(inclination);
    simulator.truth.orbit.velocity_eci_m_s[0] =
        -radius * (float)mean_motion * (float)sin(phase);
    simulator.truth.orbit.velocity_eci_m_s[1] =
        radius * (float)mean_motion * (float)cos(phase) * cosf(inclination);
    simulator.truth.orbit.velocity_eci_m_s[2] =
        radius * (float)mean_motion * (float)cos(phase) * sinf(inclination);
    simulator.truth.orbit.valid = 1U;

    (void)adcs_reference_sun_vector(simulator.truth.unix_time_us, sun_eci);
    (void)adcs_reference_magnetic_field(
        simulator.truth.unix_time_us,
        simulator.truth.orbit.position_eci_m,
        field_eci);
    quaternion_rotate_vector(
        simulator.truth.quaternion,
        sun_eci,
        simulator.truth.sun_vector_body);
    quaternion_rotate_vector(
        simulator.truth.quaternion,
        field_eci,
        simulator.truth.magnetic_field_body_t);
}

adcs_result_t adcs_simulator_init(const adcs_simulator_config_t *config) {
    adcs_simulator_config_t selected = config == NULL ? default_config() : *config;

    if (!config_is_valid(&selected)) {
        return ADCS_RESULT_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&simulator_lock);
    memset(&simulator, 0, sizeof(simulator));
    simulator.config = selected;
    (void)adcs_quaternion_normalize(
        selected.initial_quaternion,
        simulator.truth.quaternion);
    memcpy(
        simulator.truth.angular_rate_rad_s,
        selected.initial_angular_rate_rad_s,
        sizeof(simulator.truth.angular_rate_rad_s));
    simulator.truth.unix_time_us = selected.initial_unix_time_us;
    simulator.truth.actuator.enabled = 0U;
    simulator.truth.reaction_wheels.enabled = 0U;
    simulator.initialized = 1U;
    update_environment_locked();
    pthread_mutex_unlock(&simulator_lock);
    return ADCS_RESULT_OK;
}

adcs_result_t adcs_simulator_step(float dt_s) {
    float torque[ADCS_VECTOR_LENGTH];
    float momentum[ADCS_VECTOR_LENGTH];
    float gyroscopic[ADCS_VECTOR_LENGTH];
    float angular_acceleration[ADCS_VECTOR_LENGTH];
    float negative_rotation[ADCS_VECTOR_LENGTH];
    float rotation_angle;
    versor delta;
    versor updated;
    uint64_t step_us;

    if (!isfinite(dt_s) || dt_s <= 0.0F || dt_s > 1.0F) {
        return ADCS_RESULT_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&simulator_lock);
    if (simulator.initialized == 0U) {
        pthread_mutex_unlock(&simulator_lock);
        return ADCS_RESULT_NOT_INITIALIZED;
    }

    adcs_vector_cross(
        simulator.truth.actuator.dipole_a_m2,
        simulator.truth.magnetic_field_body_t,
        torque);
    if (simulator.truth.actuator.enabled == 0U ||
        (simulator.faults & ADCS_SIM_FAULT_ACTUATOR) != 0U) {
        memset(torque, 0, sizeof(torque));
    }

    if (simulator.truth.reaction_wheels.enabled != 0U &&
        (simulator.faults & ADCS_SIM_FAULT_ACTUATOR) == 0U) {
        for (size_t axis = 0U; axis < ADCS_VECTOR_LENGTH; ++axis) {
            float wheel_torque = simulator.truth.reaction_wheels.body_torque_nm[axis];
            float next_momentum =
                simulator.truth.reaction_wheel_momentum_nms[axis] -
                wheel_torque * dt_s;
            float limited_momentum = adcs_clampf(
                next_momentum,
                -simulator.config.maximum_reaction_wheel_momentum_nms,
                simulator.config.maximum_reaction_wheel_momentum_nms);

            if (limited_momentum != next_momentum) {
                wheel_torque =
                    (simulator.truth.reaction_wheel_momentum_nms[axis] -
                     limited_momentum) / dt_s;
            }
            simulator.truth.reaction_wheel_momentum_nms[axis] = limited_momentum;
            torque[axis] += wheel_torque;
        }
    }

    for (size_t axis = 0U; axis < ADCS_VECTOR_LENGTH; ++axis) {
        momentum[axis] = simulator.config.inertia_kg_m2[axis] *
                         simulator.truth.angular_rate_rad_s[axis];
    }
    adcs_vector_cross(simulator.truth.angular_rate_rad_s, momentum, gyroscopic);
    for (size_t axis = 0U; axis < ADCS_VECTOR_LENGTH; ++axis) {
        angular_acceleration[axis] =
            (torque[axis] - gyroscopic[axis] -
             SIM_VISCOUS_DAMPING_NM_S *
                 simulator.truth.angular_rate_rad_s[axis]) /
            simulator.config.inertia_kg_m2[axis];
        simulator.truth.angular_rate_rad_s[axis] +=
            angular_acceleration[axis] * dt_s;
        simulator.truth.angular_rate_rad_s[axis] = adcs_clampf(
            simulator.truth.angular_rate_rad_s[axis],
            -SIM_RATE_LIMIT_RAD_S,
            SIM_RATE_LIMIT_RAD_S);
        negative_rotation[axis] =
            -simulator.truth.angular_rate_rad_s[axis] * dt_s;
    }

    rotation_angle = adcs_vector_norm(negative_rotation);
    if (rotation_angle > 1.0e-8F) {
        float half_angle = 0.5F * rotation_angle;
        float scale = sinf(half_angle) / rotation_angle;
        delta[0] = negative_rotation[0] * scale;
        delta[1] = negative_rotation[1] * scale;
        delta[2] = negative_rotation[2] * scale;
        delta[3] = cosf(half_angle);
        quaternion_multiply(delta, simulator.truth.quaternion, updated);
        (void)adcs_quaternion_normalize(updated, simulator.truth.quaternion);
    }

    step_us = (uint64_t)llround((double)dt_s * 1000000.0);
    if (step_us == 0U) {
        step_us = 1U;
    }
    simulator.truth.timestamp_us += step_us;
    simulator.truth.unix_time_us += step_us;
    update_environment_locked();
    pthread_mutex_unlock(&simulator_lock);
    return ADCS_RESULT_OK;
}

adcs_result_t adcs_simulator_read_imu(
    uint64_t *timestamp_us,
    float angular_rate_rad_s[ADCS_VECTOR_LENGTH],
    float acceleration_m_s2[ADCS_VECTOR_LENGTH]) {
    double phase;

    if (timestamp_us == NULL || angular_rate_rad_s == NULL ||
        acceleration_m_s2 == NULL) {
        return ADCS_RESULT_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&simulator_lock);
    if (simulator.initialized == 0U ||
        (simulator.faults & ADCS_SIM_FAULT_IMU) != 0U) {
        pthread_mutex_unlock(&simulator_lock);
        return ADCS_RESULT_UNAVAILABLE;
    }
    phase = (double)simulator.truth.timestamp_us / 1000000.0;
    *timestamp_us = simulator.truth.timestamp_us;
    for (size_t axis = 0U; axis < ADCS_VECTOR_LENGTH; ++axis) {
        angular_rate_rad_s[axis] = simulator.truth.angular_rate_rad_s[axis] +
            simulator.config.gyro_bias_rad_s[axis] +
            0.00005F * (float)sin(phase * (1.7 + (double)axis));
        acceleration_m_s2[axis] = 0.0F;
    }
    pthread_mutex_unlock(&simulator_lock);
    return ADCS_RESULT_OK;
}

adcs_result_t adcs_simulator_read_magnetometer(
    uint64_t *timestamp_us,
    float magnetic_field_t[ADCS_VECTOR_LENGTH]) {
    double phase;

    if (timestamp_us == NULL || magnetic_field_t == NULL) {
        return ADCS_RESULT_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&simulator_lock);
    if (simulator.initialized == 0U ||
        (simulator.faults & ADCS_SIM_FAULT_MAGNETOMETER) != 0U) {
        pthread_mutex_unlock(&simulator_lock);
        return ADCS_RESULT_UNAVAILABLE;
    }
    phase = (double)simulator.truth.timestamp_us / 1000000.0;
    *timestamp_us = simulator.truth.timestamp_us;
    for (size_t axis = 0U; axis < ADCS_VECTOR_LENGTH; ++axis) {
        magnetic_field_t[axis] = simulator.truth.magnetic_field_body_t[axis] +
            2.0e-8F * (float)sin(phase * (0.9 + (double)axis));
    }
    pthread_mutex_unlock(&simulator_lock);
    return ADCS_RESULT_OK;
}

adcs_result_t adcs_simulator_read_sun_sensor(
    uint64_t *timestamp_us,
    float sun_vector_body[ADCS_VECTOR_LENGTH],
    float *irradiance_w_m2) {
    float noisy_vector[ADCS_VECTOR_LENGTH];
    double phase;

    if (timestamp_us == NULL || sun_vector_body == NULL || irradiance_w_m2 == NULL) {
        return ADCS_RESULT_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&simulator_lock);
    if (simulator.initialized == 0U ||
        (simulator.faults & ADCS_SIM_FAULT_SUN_SENSOR) != 0U) {
        pthread_mutex_unlock(&simulator_lock);
        return ADCS_RESULT_UNAVAILABLE;
    }
    phase = (double)simulator.truth.timestamp_us / 1000000.0;
    *timestamp_us = simulator.truth.timestamp_us;
    for (size_t axis = 0U; axis < ADCS_VECTOR_LENGTH; ++axis) {
        noisy_vector[axis] = simulator.truth.sun_vector_body[axis] +
            0.0005F * (float)sin(phase * (0.5 + (double)axis));
    }
    (void)adcs_vector_normalize(noisy_vector, sun_vector_body);
    *irradiance_w_m2 = 1361.0F;
    pthread_mutex_unlock(&simulator_lock);
    return ADCS_RESULT_OK;
}

adcs_result_t adcs_simulator_read_thermistor(
    uint64_t *timestamp_us,
    float *temperature_c) {
    double phase;

    if (timestamp_us == NULL || temperature_c == NULL) {
        return ADCS_RESULT_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&simulator_lock);
    if (simulator.initialized == 0U ||
        (simulator.faults & ADCS_SIM_FAULT_THERMISTOR) != 0U) {
        pthread_mutex_unlock(&simulator_lock);
        return ADCS_RESULT_UNAVAILABLE;
    }
    phase = (double)simulator.truth.timestamp_us / 1000000.0;
    *timestamp_us = simulator.truth.timestamp_us;
    *temperature_c = 24.0F + 1.5F * (float)sin(phase / 600.0);
    pthread_mutex_unlock(&simulator_lock);
    return ADCS_RESULT_OK;
}

adcs_result_t adcs_simulator_set_magnetorquer(
    const adcs_magnetorquer_command_t *command) {
    if (command == NULL ||
        !adcs_values_are_finite(command->dipole_a_m2, ADCS_VECTOR_LENGTH)) {
        return ADCS_RESULT_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&simulator_lock);
    if (simulator.initialized == 0U) {
        pthread_mutex_unlock(&simulator_lock);
        return ADCS_RESULT_NOT_INITIALIZED;
    }
    if ((simulator.faults & ADCS_SIM_FAULT_ACTUATOR) != 0U) {
        memset(&simulator.truth.actuator, 0, sizeof(simulator.truth.actuator));
        pthread_mutex_unlock(&simulator_lock);
        return ADCS_RESULT_UNAVAILABLE;
    }

    simulator.truth.actuator = *command;
    for (size_t axis = 0U; axis < ADCS_VECTOR_LENGTH; ++axis) {
        simulator.truth.actuator.dipole_a_m2[axis] = adcs_clampf(
            simulator.truth.actuator.dipole_a_m2[axis],
            -simulator.config.maximum_dipole_a_m2,
            simulator.config.maximum_dipole_a_m2);
    }
    if (command->enabled == 0U) {
        memset(
            simulator.truth.actuator.dipole_a_m2,
            0,
            sizeof(simulator.truth.actuator.dipole_a_m2));
    }
    pthread_mutex_unlock(&simulator_lock);
    return ADCS_RESULT_OK;
}

adcs_result_t adcs_simulator_set_reaction_wheels(
    const adcs_reaction_wheel_command_t *command) {
    if (command == NULL ||
        !adcs_values_are_finite(command->body_torque_nm, ADCS_VECTOR_LENGTH)) {
        return ADCS_RESULT_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&simulator_lock);
    if (simulator.initialized == 0U) {
        pthread_mutex_unlock(&simulator_lock);
        return ADCS_RESULT_NOT_INITIALIZED;
    }
    if ((simulator.faults & ADCS_SIM_FAULT_ACTUATOR) != 0U) {
        memset(
            &simulator.truth.reaction_wheels,
            0,
            sizeof(simulator.truth.reaction_wheels));
        pthread_mutex_unlock(&simulator_lock);
        return ADCS_RESULT_UNAVAILABLE;
    }

    simulator.truth.reaction_wheels = *command;
    for (size_t axis = 0U; axis < ADCS_VECTOR_LENGTH; ++axis) {
        simulator.truth.reaction_wheels.body_torque_nm[axis] = adcs_clampf(
            simulator.truth.reaction_wheels.body_torque_nm[axis],
            -simulator.config.maximum_reaction_wheel_torque_nm,
            simulator.config.maximum_reaction_wheel_torque_nm);
    }
    if (command->enabled == 0U) {
        memset(
            simulator.truth.reaction_wheels.body_torque_nm,
            0,
            sizeof(simulator.truth.reaction_wheels.body_torque_nm));
    }
    pthread_mutex_unlock(&simulator_lock);
    return ADCS_RESULT_OK;
}

adcs_result_t adcs_simulator_get_truth(adcs_simulator_truth_t *truth) {
    if (truth == NULL) {
        return ADCS_RESULT_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&simulator_lock);
    if (simulator.initialized == 0U) {
        pthread_mutex_unlock(&simulator_lock);
        return ADCS_RESULT_NOT_INITIALIZED;
    }
    *truth = simulator.truth;
    pthread_mutex_unlock(&simulator_lock);
    return ADCS_RESULT_OK;
}

adcs_result_t adcs_simulator_get_orbit(adcs_orbit_state_t *orbit) {
    if (orbit == NULL) {
        return ADCS_RESULT_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&simulator_lock);
    if (simulator.initialized == 0U) {
        pthread_mutex_unlock(&simulator_lock);
        return ADCS_RESULT_NOT_INITIALIZED;
    }
    *orbit = simulator.truth.orbit;
    pthread_mutex_unlock(&simulator_lock);
    return ADCS_RESULT_OK;
}

uint64_t adcs_simulator_get_time_us(void) {
    uint64_t timestamp;

    pthread_mutex_lock(&simulator_lock);
    timestamp = simulator.truth.timestamp_us;
    pthread_mutex_unlock(&simulator_lock);
    return timestamp;
}

uint64_t adcs_simulator_get_unix_time_us(void) {
    uint64_t timestamp;

    pthread_mutex_lock(&simulator_lock);
    timestamp = simulator.truth.unix_time_us;
    pthread_mutex_unlock(&simulator_lock);
    return timestamp;
}

adcs_result_t adcs_simulator_set_unix_time(uint64_t unix_time_us) {
    if (unix_time_us == 0U) {
        return ADCS_RESULT_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&simulator_lock);
    if (simulator.initialized == 0U) {
        pthread_mutex_unlock(&simulator_lock);
        return ADCS_RESULT_NOT_INITIALIZED;
    }
    simulator.truth.unix_time_us = unix_time_us;
    update_environment_locked();
    pthread_mutex_unlock(&simulator_lock);
    return ADCS_RESULT_OK;
}

void adcs_simulator_set_faults(uint32_t fault_mask) {
    pthread_mutex_lock(&simulator_lock);
    simulator.faults = fault_mask &
        (ADCS_SIM_FAULT_IMU | ADCS_SIM_FAULT_MAGNETOMETER |
         ADCS_SIM_FAULT_SUN_SENSOR | ADCS_SIM_FAULT_THERMISTOR |
         ADCS_SIM_FAULT_ACTUATOR);
    pthread_mutex_unlock(&simulator_lock);
}

uint32_t adcs_simulator_get_faults(void) {
    uint32_t faults;

    pthread_mutex_lock(&simulator_lock);
    faults = simulator.faults;
    pthread_mutex_unlock(&simulator_lock);
    return faults;
}
