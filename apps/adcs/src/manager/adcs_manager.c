#include "manager/adcs_manager.h"

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "control/control_math.h"
#include "guidance/target_generator.h"
#include "manager/fault_manager.h"

#define MANAGER_DEFAULT_MAXIMUM_TARGET_RATE_RAD_S 0.08F
#define MANAGER_SUN_SETTLED_ERROR_RAD 0.174532925F

static adcs_manager_state_t state;
static adcs_manager_config_t manager_config;
static pthread_mutex_t manager_lock = PTHREAD_MUTEX_INITIALIZER;

static adcs_manager_config_t default_config(void) {
    return (adcs_manager_config_t) {
        .detumble_entry_rate_rad_s = 0.03F,
        .detumble_exit_rate_rad_s = 0.008F,
        .excessive_rate_rad_s = 0.75F,
        .minimum_estimator_confidence = 0.35F,
        .detumble_settle_cycles = 20U,
        .sun_acquisition_settle_cycles = 20U,
        .sensor_stale_after_us = 250000U,
        .estimate_stale_after_us = 250000U
    };
}

static uint8_t mode_is_valid(adcs_mode_t mode) {
    return mode >= ADCS_MODE_BOOT && mode < ADCS_MODE_COUNT;
}

static uint8_t quaternion_is_valid(const versor quaternion) {
    versor normalized;
    return adcs_quaternion_normalize(quaternion, normalized) == ADCS_RESULT_OK;
}

static uint8_t sensors_ready_locked(void) {
    uint32_t required = ADCS_SENSOR_VALID_GYROSCOPE |
                        ADCS_SENSOR_VALID_MAGNETOMETER;
    return state.latest_sensors.timestamp_us != 0U &&
           (state.latest_sensors.valid_mask & required) == required;
}

static uint8_t attitude_ready_locked(void) {
    return state.latest_attitude.valid != 0U &&
           state.latest_attitude.timestamp_us != 0U &&
           state.latest_attitude.confidence >=
               manager_config.minimum_estimator_confidence &&
           quaternion_is_valid(state.latest_attitude.quaternion);
}

static uint8_t state_is_fresh_locked(void) {
    uint64_t now = state.latest_sensors.timestamp_us;

    if (now == 0U || state.latest_attitude.timestamp_us == 0U ||
        state.latest_attitude.timestamp_us > now) {
        return 0U;
    }
    return now - state.latest_sensors.timestamp_us <=
               manager_config.sensor_stale_after_us &&
           now - state.latest_attitude.timestamp_us <=
               manager_config.estimate_stale_after_us;
}

static uint8_t transition_is_allowed_locked(adcs_mode_t requested) {
    switch (requested) {
        case ADCS_MODE_SAFE:
            return 1U;
        case ADCS_MODE_DETUMBLE:
            return sensors_ready_locked();
        case ADCS_MODE_SUN_ACQUISITION:
        case ADCS_MODE_SUN_POINTING:
            return attitude_ready_locked() &&
                   (state.latest_sensors.valid_mask & ADCS_SENSOR_VALID_SUN) != 0U &&
                   (state.latest_references.valid_mask & ADCS_REFERENCE_VALID_SUN) != 0U;
        case ADCS_MODE_EARTH_POINTING:
            return attitude_ready_locked() &&
                   (state.latest_references.valid_mask & ADCS_REFERENCE_VALID_NADIR) != 0U;
        case ADCS_MODE_SLEWING:
        case ADCS_MODE_TARGET_POINTING:
        case ADCS_MODE_SCIENCE:
            return attitude_ready_locked() &&
                   quaternion_is_valid(state.guidance_target.target_quaternion);
        case ADCS_MODE_BOOT:
        case ADCS_MODE_COUNT:
        default:
            return 0U;
    }
}

static void enter_mode_locked(adcs_mode_t mode) {
    if (state.mode == mode) {
        return;
    }

    printf("[ADCS MANAGER] %s -> %s\n",
           adcs_manager_mode_name(state.mode),
           adcs_manager_mode_name(mode));
    fflush(stdout);
    state.mode = mode;
    state.mode_entered_us = state.latest_sensors.timestamp_us;
    state.stable_cycles = 0U;
    ++state.health.mode_transitions;
}

static void refresh_builtin_target_locked(void) {
    const float sun_axis[ADCS_VECTOR_LENGTH] = {1.0F, 0.0F, 0.0F};
    const float nadir_axis[ADCS_VECTOR_LENGTH] = {0.0F, 0.0F, 1.0F};
    adcs_guidance_target_t target;

    if ((state.mode == ADCS_MODE_SUN_ACQUISITION ||
         state.mode == ADCS_MODE_SUN_POINTING) &&
        (state.latest_references.valid_mask & ADCS_REFERENCE_VALID_SUN) != 0U &&
        adcs_guidance_target_from_vector(
            state.mode,
            sun_axis,
            state.latest_references.sun_eci_unit,
            MANAGER_DEFAULT_MAXIMUM_TARGET_RATE_RAD_S,
            &target) == ADCS_RESULT_OK) {
        state.guidance_target = target;
    } else if (state.mode == ADCS_MODE_EARTH_POINTING &&
               (state.latest_references.valid_mask & ADCS_REFERENCE_VALID_NADIR) != 0U &&
               adcs_guidance_target_from_vector(
                   state.mode,
                   nadir_axis,
                   state.latest_references.nadir_eci_unit,
                   MANAGER_DEFAULT_MAXIMUM_TARGET_RATE_RAD_S,
                   &target) == ADCS_RESULT_OK) {
        state.guidance_target = target;
    }
}

void adcs_manager_init(void) {
    pthread_mutex_lock(&manager_lock);
    memset(&state, 0, sizeof(state));
    manager_config = default_config();
    state.mode = ADCS_MODE_BOOT;
    state.requested_mode = ADCS_MODE_SUN_ACQUISITION;
    state.latest_attitude.quaternion[3] = 1.0F;
    state.guidance_target.target_quaternion[3] = 1.0F;
    state.guidance_target.pointing_axis_body[0] = 1.0F;
    state.guidance_target.maximum_rate_rad_s =
        MANAGER_DEFAULT_MAXIMUM_TARGET_RATE_RAD_S;
    state.health.sensors_healthy = 0U;
    state.health.estimator_healthy = 0U;
    state.health.actuators_healthy = 1U;
    state.health.minimum_stack_margin_words = 0.0F;
    state.initialized = 1U;
    pthread_mutex_unlock(&manager_lock);
}

void adcs_manager_configure(const adcs_manager_config_t *config) {
    if (config == NULL ||
        !isfinite(config->detumble_entry_rate_rad_s) ||
        !isfinite(config->detumble_exit_rate_rad_s) ||
        !isfinite(config->excessive_rate_rad_s) ||
        !isfinite(config->minimum_estimator_confidence) ||
        config->detumble_exit_rate_rad_s < 0.0F ||
        config->detumble_entry_rate_rad_s <= config->detumble_exit_rate_rad_s ||
        config->excessive_rate_rad_s <= config->detumble_entry_rate_rad_s ||
        config->minimum_estimator_confidence < 0.0F ||
        config->minimum_estimator_confidence > 1.0F ||
        config->detumble_settle_cycles == 0U ||
        config->sun_acquisition_settle_cycles == 0U ||
        config->sensor_stale_after_us == 0U ||
        config->estimate_stale_after_us == 0U) {
        return;
    }

    pthread_mutex_lock(&manager_lock);
    manager_config = *config;
    pthread_mutex_unlock(&manager_lock);
}

void adcs_manager_update(void) {
    uint32_t critical_faults;
    float body_rate;

    pthread_mutex_lock(&manager_lock);
    if (state.initialized == 0U) {
        pthread_mutex_unlock(&manager_lock);
        return;
    }

    state.health.active_faults = fault_management_get_active();
    critical_faults = state.health.active_faults &
        (ADCS_FAULT_SENSOR_STALE | ADCS_FAULT_SENSOR_RANGE |
         ADCS_FAULT_ATTITUDE_INVALID | ADCS_FAULT_EXCESSIVE_RATE |
         ADCS_FAULT_ACTUATOR | ADCS_FAULT_TASK_DEADLINE);
    if (critical_faults != 0U) {
        enter_mode_locked(ADCS_MODE_SAFE);
        refresh_builtin_target_locked();
        pthread_mutex_unlock(&manager_lock);
        return;
    }

    if (state.mode == ADCS_MODE_BOOT) {
        if (sensors_ready_locked() && attitude_ready_locked() &&
            state_is_fresh_locked()) {
            enter_mode_locked(ADCS_MODE_SAFE);
        } else {
            pthread_mutex_unlock(&manager_lock);
            return;
        }
    }

    body_rate = adcs_vector_norm(state.latest_attitude.angular_rate_rad_s);
    if (!isfinite(body_rate) || body_rate > manager_config.excessive_rate_rad_s) {
        enter_mode_locked(ADCS_MODE_SAFE);
        pthread_mutex_unlock(&manager_lock);
        return;
    }

    if (state.requested_mode == ADCS_MODE_SAFE) {
        enter_mode_locked(ADCS_MODE_SAFE);
    } else if (body_rate > manager_config.detumble_entry_rate_rad_s) {
        enter_mode_locked(ADCS_MODE_DETUMBLE);
    } else if (state.mode != ADCS_MODE_DETUMBLE &&
               state.requested_mode != state.mode &&
               transition_is_allowed_locked(state.requested_mode)) {
        enter_mode_locked(state.requested_mode);
    }

    switch (state.mode) {
        case ADCS_MODE_SAFE:
            if (state.requested_mode != ADCS_MODE_SAFE) {
                if (body_rate > manager_config.detumble_entry_rate_rad_s &&
                    sensors_ready_locked()) {
                    enter_mode_locked(ADCS_MODE_DETUMBLE);
                } else if (transition_is_allowed_locked(state.requested_mode)) {
                    enter_mode_locked(state.requested_mode);
                }
            }
            break;
        case ADCS_MODE_DETUMBLE:
            if (body_rate <= manager_config.detumble_exit_rate_rad_s) {
                ++state.stable_cycles;
                if (state.stable_cycles >= manager_config.detumble_settle_cycles) {
                    state.requested_mode = ADCS_MODE_SUN_ACQUISITION;
                    if (transition_is_allowed_locked(ADCS_MODE_SUN_ACQUISITION)) {
                        enter_mode_locked(ADCS_MODE_SUN_ACQUISITION);
                    }
                }
            } else {
                state.stable_cycles = 0U;
            }
            break;
        case ADCS_MODE_SUN_ACQUISITION:
            if (state.latest_control.pointing_error_rad <=
                    MANAGER_SUN_SETTLED_ERROR_RAD &&
                body_rate <= manager_config.detumble_entry_rate_rad_s) {
                ++state.stable_cycles;
                if (state.stable_cycles >=
                    manager_config.sun_acquisition_settle_cycles) {
                    state.requested_mode = ADCS_MODE_SUN_POINTING;
                    enter_mode_locked(ADCS_MODE_SUN_POINTING);
                }
            } else {
                state.stable_cycles = 0U;
            }
            break;
        case ADCS_MODE_SLEWING:
            if (state.latest_control.target_settled != 0U) {
                state.requested_mode = ADCS_MODE_TARGET_POINTING;
                enter_mode_locked(ADCS_MODE_TARGET_POINTING);
            }
            break;
        case ADCS_MODE_SUN_POINTING:
        case ADCS_MODE_EARTH_POINTING:
        case ADCS_MODE_TARGET_POINTING:
        case ADCS_MODE_SCIENCE:
        case ADCS_MODE_BOOT:
        case ADCS_MODE_COUNT:
        default:
            break;
    }

    refresh_builtin_target_locked();
    pthread_mutex_unlock(&manager_lock);
}

int adcs_manager_get_mode(void) {
    int mode;

    pthread_mutex_lock(&manager_lock);
    mode = (int)state.mode;
    pthread_mutex_unlock(&manager_lock);
    return mode;
}

void adcs_manager_request_mode(int mode) {
    pthread_mutex_lock(&manager_lock);
    if (mode_is_valid((adcs_mode_t)mode) && mode != ADCS_MODE_BOOT) {
        state.requested_mode = (adcs_mode_t)mode;
    } else {
        ++state.health.rejected_commands;
    }
    pthread_mutex_unlock(&manager_lock);
}

void adcs_manager_set_sensors(const adcs_sensor_packet_t *sensors) {
    if (sensors == NULL) {
        return;
    }

    pthread_mutex_lock(&manager_lock);
    if (sensors->timestamp_us >= state.latest_sensors.timestamp_us) {
        state.latest_sensors = *sensors;
        state.health.sensors_healthy =
            (sensors->valid_mask &
             (ADCS_SENSOR_VALID_GYROSCOPE | ADCS_SENSOR_VALID_MAGNETOMETER)) ==
            (ADCS_SENSOR_VALID_GYROSCOPE | ADCS_SENSOR_VALID_MAGNETOMETER);
    }
    pthread_mutex_unlock(&manager_lock);
}

void adcs_manager_set_references(
    const adcs_orbit_state_t *orbit,
    const adcs_reference_vectors_t *references) {
    pthread_mutex_lock(&manager_lock);
    if (orbit != NULL) {
        state.latest_orbit = *orbit;
    }
    if (references != NULL) {
        state.latest_references = *references;
    }
    pthread_mutex_unlock(&manager_lock);
}

void adcs_manager_set_attitude(const adcs_attitude_state_t *attitude) {
    adcs_attitude_state_t validated;

    if (attitude == NULL) {
        return;
    }
    validated = *attitude;
    if (adcs_quaternion_normalize(
            attitude->quaternion,
            validated.quaternion) != ADCS_RESULT_OK) {
        memset(validated.quaternion, 0, sizeof(validated.quaternion));
        validated.quaternion[3] = 1.0F;
        validated.valid = 0U;
    }

    pthread_mutex_lock(&manager_lock);
    if (validated.timestamp_us >= state.latest_attitude.timestamp_us) {
        state.latest_attitude = validated;
        state.health.estimator_healthy = validated.valid;
    }
    pthread_mutex_unlock(&manager_lock);
}

void adcs_manager_set_control_output(const adcs_control_output_t *control) {
    if (control == NULL) {
        return;
    }

    pthread_mutex_lock(&manager_lock);
    if (control->timestamp_us >= state.latest_control.timestamp_us) {
        state.latest_control = *control;
    }
    pthread_mutex_unlock(&manager_lock);
}

void adcs_manager_set_guidance_target(const adcs_guidance_target_t *target) {
    adcs_guidance_target_t validated;

    if (target == NULL || !mode_is_valid(target->mode) ||
        !isfinite(target->maximum_rate_rad_s) ||
        target->maximum_rate_rad_s <= 0.0F) {
        adcs_manager_note_rejected_command();
        return;
    }
    validated = *target;
    if (adcs_quaternion_normalize(
            target->target_quaternion,
            validated.target_quaternion) != ADCS_RESULT_OK) {
        adcs_manager_note_rejected_command();
        return;
    }

    pthread_mutex_lock(&manager_lock);
    state.guidance_target = validated;
    pthread_mutex_unlock(&manager_lock);
}

void adcs_manager_set_actuators_inhibited(uint8_t inhibited) {
    pthread_mutex_lock(&manager_lock);
    state.actuators_inhibited = inhibited != 0U;
    pthread_mutex_unlock(&manager_lock);
}

void adcs_manager_set_health(const adcs_health_t *health) {
    uint32_t rejected;
    uint32_t dropped;
    uint32_t resets;
    uint32_t control_errors;
    uint32_t transitions;

    if (health == NULL) {
        return;
    }

    pthread_mutex_lock(&manager_lock);
    rejected = state.health.rejected_commands;
    dropped = state.health.dropped_messages;
    resets = state.health.estimator_resets;
    control_errors = state.health.controller_errors;
    transitions = state.health.mode_transitions;
    state.health = *health;
    state.health.rejected_commands = rejected;
    state.health.dropped_messages = dropped;
    state.health.estimator_resets = resets;
    state.health.controller_errors = control_errors;
    state.health.mode_transitions = transitions;
    pthread_mutex_unlock(&manager_lock);
}

void adcs_manager_note_rejected_command(void) {
    pthread_mutex_lock(&manager_lock);
    ++state.health.rejected_commands;
    pthread_mutex_unlock(&manager_lock);
}

void adcs_manager_note_dropped_message(void) {
    pthread_mutex_lock(&manager_lock);
    ++state.health.dropped_messages;
    pthread_mutex_unlock(&manager_lock);
}

void adcs_manager_note_estimator_reset(void) {
    pthread_mutex_lock(&manager_lock);
    ++state.health.estimator_resets;
    pthread_mutex_unlock(&manager_lock);
}

void adcs_manager_note_controller_error(void) {
    pthread_mutex_lock(&manager_lock);
    ++state.health.controller_errors;
    pthread_mutex_unlock(&manager_lock);
}

void adcs_manager_set_legacy_position(int32_t position) {
    pthread_mutex_lock(&manager_lock);
    state.last_commanded_position = position;
    pthread_mutex_unlock(&manager_lock);
}

const char *adcs_manager_mode_name(adcs_mode_t mode) {
    static const char *const names[ADCS_MODE_COUNT] = {
        "BOOT",
        "SAFE",
        "DETUMBLE",
        "SUN_ACQUISITION",
        "SUN_POINTING",
        "EARTH_POINTING",
        "SLEWING",
        "TARGET_POINTING",
        "SCIENCE"
    };

    return mode_is_valid(mode) ? names[mode] : "INVALID";
}

void adcs_manager_get_state(adcs_manager_state_t *state_out) {
    if (state_out == NULL) {
        return;
    }

    pthread_mutex_lock(&manager_lock);
    *state_out = state;
    pthread_mutex_unlock(&manager_lock);
}
