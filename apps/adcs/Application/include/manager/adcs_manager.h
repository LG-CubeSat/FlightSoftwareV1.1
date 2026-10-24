/*
 * ADCS mode manager.
 *
 * The manager owns mode transitions and the latest coherent subsystem state;
 * it does not perform estimation or control math. Nominal autonomous flow is
 * BOOT -> SAFE -> DETUMBLE -> SUN_ACQUISITION -> SUN_POINTING. Earth pointing,
 * science, and explicit slews require a valid command and guidance target.
 */
#ifndef ADCS_MANAGER_ADCS_MANAGER_H
#define ADCS_MANAGER_ADCS_MANAGER_H

#include <stdint.h>

#include "communication/message.h"

/* Compatibility names retained for the existing task and command code. */
#define ADCS_STATE_IDLE           ADCS_MODE_SAFE
#define ADCS_STATE_DETUMBLING     ADCS_MODE_DETUMBLE
#define ADCS_STATE_SUN_POINTING   ADCS_MODE_SUN_POINTING
#define ADCS_STATE_EARTH_POINTING ADCS_MODE_EARTH_POINTING
#define ADCS_STATE_SCIENCE        ADCS_MODE_SCIENCE
#define ADCS_STATE_SLEWING        ADCS_MODE_SLEWING
#define ADCS_STATE_TARGET_POINTING ADCS_MODE_TARGET_POINTING

typedef struct {
    float detumble_entry_rate_rad_s;
    float detumble_exit_rate_rad_s;
    float excessive_rate_rad_s;
    float minimum_estimator_confidence;
    uint32_t detumble_settle_cycles;
    uint32_t sun_acquisition_settle_cycles;
    uint64_t sensor_stale_after_us;
    uint64_t estimate_stale_after_us;
} adcs_manager_config_t;

typedef struct {
    adcs_mode_t mode;
    adcs_mode_t requested_mode;
    int32_t last_commanded_position;
    adcs_sensor_packet_t latest_sensors;
    adcs_orbit_state_t latest_orbit;
    adcs_reference_vectors_t latest_references;
    adcs_attitude_state_t latest_attitude;
    adcs_control_output_t latest_control;
    adcs_guidance_target_t guidance_target;
    adcs_health_t health;
    uint64_t mode_entered_us;
    uint32_t stable_cycles;
    uint8_t actuators_inhibited;
    uint8_t initialized;
} adcs_manager_state_t;

/* Initializes BOOT state and requests the nominal autonomous Sun-pointing path. */
void adcs_manager_init(void);

/* Replaces default transition thresholds before the scheduler starts. */
void adcs_manager_configure(const adcs_manager_config_t *config);

/* Evaluates pending requests, sensor health, faults, and autonomous transitions.
   now_us is a real wall/sim-clock timestamp, independent of sensor/attitude
   timestamps, used to judge whether that data is actually fresh. */
void adcs_manager_update(uint64_t now_us);

/* Returns the active mode using the compatibility integer API. */
int adcs_manager_get_mode(void);

/* Records a requested mode; update() validates the transition before activation. */
void adcs_manager_request_mode(int mode);

/* Supplies the newest sensor packet used for health and transition decisions. */
void adcs_manager_set_sensors(const adcs_sensor_packet_t *sensors);

/* Supplies the orbit and inertial references paired with the latest estimate. */
void adcs_manager_set_references(
    const adcs_orbit_state_t *orbit,
    const adcs_reference_vectors_t *references);

/* Supplies the newest estimator state used for health and transition decisions. */
void adcs_manager_set_attitude(const adcs_attitude_state_t *attitude);

/* Supplies the newest control result for telemetry and actuator-health tracking. */
void adcs_manager_set_control_output(const adcs_control_output_t *control);

/* Updates the commanded attitude/vector target without changing mode directly. */
void adcs_manager_set_guidance_target(const adcs_guidance_target_t *target);

/* Inhibits or permits all actuator output independently of active mode. */
void adcs_manager_set_actuators_inhibited(uint8_t inhibited);

/* Stores health counters and flags maintained by housekeeping and tasks. */
void adcs_manager_set_health(const adcs_health_t *health);

/* Records command/queue/controller events in the manager-owned health state. */
void adcs_manager_note_rejected_command(void);
void adcs_manager_note_dropped_message(void);
void adcs_manager_note_estimator_reset(void);
void adcs_manager_note_controller_error(void);

/* Retains compatibility state for the repository's legacy command test. */
void adcs_manager_set_legacy_position(int32_t position);

/* Returns a stable printable name for logs and telemetry diagnostics. */
const char *adcs_manager_mode_name(adcs_mode_t mode);

/* Copies one coherent manager snapshot for control or telemetry consumers. */
void adcs_manager_get_state(adcs_manager_state_t *state_out);

#endif
