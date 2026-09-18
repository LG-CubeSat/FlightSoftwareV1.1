/* Deterministic rigid-body and sensor simulation used by the ADCS executable. */
#ifndef ADCS_SIMULATION_ADCS_SIMULATOR_H
#define ADCS_SIMULATION_ADCS_SIMULATOR_H

#include "communication/message.h"

typedef enum {
    ADCS_SIM_FAULT_NONE = 0U,
    ADCS_SIM_FAULT_IMU = 1U << 0,
    ADCS_SIM_FAULT_MAGNETOMETER = 1U << 1,
    ADCS_SIM_FAULT_SUN_SENSOR = 1U << 2,
    /* Bit 3 remains reserved so existing actuator fault masks stay stable. */
    ADCS_SIM_FAULT_ACTUATOR = 1U << 4
} adcs_simulator_fault_t;

typedef struct {
    versor initial_quaternion;
    float initial_angular_rate_rad_s[ADCS_VECTOR_LENGTH];
    float inertia_kg_m2[ADCS_VECTOR_LENGTH];
    float gyro_bias_rad_s[ADCS_VECTOR_LENGTH];
    float maximum_dipole_a_m2;
    float orbit_altitude_m;
    float orbit_inclination_rad;
    uint64_t initial_unix_time_us;
} adcs_simulator_config_t;

typedef struct {
    uint64_t timestamp_us;
    uint64_t unix_time_us;
    versor quaternion;
    float angular_rate_rad_s[ADCS_VECTOR_LENGTH];
    float magnetic_field_body_t[ADCS_VECTOR_LENGTH];
    float sun_vector_body[ADCS_VECTOR_LENGTH];
    adcs_orbit_state_t orbit;
    adcs_magnetorquer_command_t actuator;
} adcs_simulator_truth_t;

/* Initializes one deterministic spacecraft truth model. NULL selects defaults. */
adcs_result_t adcs_simulator_init(const adcs_simulator_config_t *config);

/* Advances orbit, attitude, and angular rate using the last actuator command. */
adcs_result_t adcs_simulator_step(float dt_s);

/* Simple hardware-shaped sensor reads derived from the shared truth state. */
adcs_result_t adcs_simulator_read_imu(
    uint64_t *timestamp_us,
    float angular_rate_rad_s[ADCS_VECTOR_LENGTH],
    float acceleration_m_s2[ADCS_VECTOR_LENGTH]);
adcs_result_t adcs_simulator_read_magnetometer(
    uint64_t *timestamp_us,
    float magnetic_field_t[ADCS_VECTOR_LENGTH]);
adcs_result_t adcs_simulator_read_sun_sensor(
    uint64_t *timestamp_us,
    float sun_vector_body[ADCS_VECTOR_LENGTH],
    float *irradiance_w_m2);

/* Applies the commanded magnetic dipole to future rigid-body steps. */
adcs_result_t adcs_simulator_set_magnetorquer(
    const adcs_magnetorquer_command_t *command);

/* Copies truth/orbit for reference generation, tests, and diagnostics. */
adcs_result_t adcs_simulator_get_truth(adcs_simulator_truth_t *truth);
adcs_result_t adcs_simulator_get_orbit(adcs_orbit_state_t *orbit);
uint64_t adcs_simulator_get_time_us(void);
uint64_t adcs_simulator_get_unix_time_us(void);

/* Time sync and deterministic fault injection controls for simulation commands. */
adcs_result_t adcs_simulator_set_unix_time(uint64_t unix_time_us);
void adcs_simulator_set_faults(uint32_t fault_mask);
uint32_t adcs_simulator_get_faults(void);

#endif
