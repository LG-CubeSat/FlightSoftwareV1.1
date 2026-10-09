#include "manager/eps_manager.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "manager/fault_manager.h"

static eps_manager_state_t state;
static eps_manager_config_t manager_config;
static pthread_mutex_t manager_lock = PTHREAD_MUTEX_INITIALIZER;

static eps_manager_config_t default_config(void) {
    return (eps_manager_config_t) {
        .low_power_state_of_charge = 0.30F,
        .safe_state_of_charge = 0.15F,
        .sensor_stale_after_us = 500000U
    };
}

static uint8_t mode_is_valid(eps_mode_t mode) {
    return mode >= EPS_MODE_BOOT && mode < EPS_MODE_COUNT;
}

static uint8_t sensors_ready_locked(void) {
    return state.latest_sensors.timestamp_us != 0U &&
           (state.latest_sensors.valid_mask & EPS_SENSOR_VALID_BATTERY) != 0U;
}

static uint8_t state_is_fresh_locked(uint64_t now_us) {
    if (now_us == 0U || state.latest_sensors.timestamp_us == 0U ||
        state.latest_sensors.timestamp_us > now_us) {
        return 0U;
    }
    return now_us - state.latest_sensors.timestamp_us <=
           manager_config.sensor_stale_after_us;
}

static void enter_mode_locked(eps_mode_t mode) {
    if (state.mode == mode) {
        return;
    }

    printf("[EPS MANAGER] %s -> %s\n",
           eps_manager_mode_name(state.mode),
           eps_manager_mode_name(mode));
    fflush(stdout);
    state.mode = mode;
    state.mode_entered_us = state.latest_sensors.timestamp_us;
    ++state.health.mode_transitions;
}

void eps_manager_init(void) {
    pthread_mutex_lock(&manager_lock);
    memset(&state, 0, sizeof(state));
    manager_config = default_config();
    state.mode = EPS_MODE_BOOT;
    state.requested_mode = EPS_MODE_NOMINAL;
    state.health.sensors_healthy = 0U;
    state.initialized = 1U;
    pthread_mutex_unlock(&manager_lock);
}

void eps_manager_configure(const eps_manager_config_t *config) {
    if (config == NULL ||
        config->low_power_state_of_charge < 0.0F ||
        config->low_power_state_of_charge > 1.0F ||
        config->safe_state_of_charge < 0.0F ||
        config->safe_state_of_charge >= config->low_power_state_of_charge ||
        config->sensor_stale_after_us == 0U) {
        return;
    }

    pthread_mutex_lock(&manager_lock);
    manager_config = *config;
    pthread_mutex_unlock(&manager_lock);
}

void eps_manager_update(uint64_t now_us) {
    uint32_t critical_faults;

    pthread_mutex_lock(&manager_lock);
    if (state.initialized == 0U) {
        pthread_mutex_unlock(&manager_lock);
        return;
    }

    state.health.active_faults = fault_management_get_active();
    critical_faults = state.health.active_faults & EPS_FAULT_ALL;
    if (critical_faults != 0U) {
        enter_mode_locked(EPS_MODE_SAFE);
        pthread_mutex_unlock(&manager_lock);
        return;
    }

    if (state.mode == EPS_MODE_BOOT) {
        if (sensors_ready_locked() && state_is_fresh_locked(now_us)) {
            enter_mode_locked(EPS_MODE_SAFE);
        } else {
            pthread_mutex_unlock(&manager_lock);
            return;
        }
    }

    if (state.requested_mode == EPS_MODE_SAFE ||
        state.latest_sensors.state_of_charge <=
            manager_config.safe_state_of_charge) {
        enter_mode_locked(EPS_MODE_SAFE);
    } else if (state.latest_sensors.state_of_charge <=
               manager_config.low_power_state_of_charge) {
        enter_mode_locked(EPS_MODE_LOW_POWER);
    } else if (state.requested_mode != state.mode) {
        enter_mode_locked(state.requested_mode);
    }

    pthread_mutex_unlock(&manager_lock);
}

int eps_manager_get_mode(void) {
    int mode;

    pthread_mutex_lock(&manager_lock);
    mode = (int)state.mode;
    pthread_mutex_unlock(&manager_lock);
    return mode;
}

void eps_manager_request_mode(int mode) {
    pthread_mutex_lock(&manager_lock);
    if (mode_is_valid((eps_mode_t)mode) && mode != EPS_MODE_BOOT) {
        state.requested_mode = (eps_mode_t)mode;
    } else {
        ++state.health.rejected_commands;
    }
    pthread_mutex_unlock(&manager_lock);
}

void eps_manager_set_sensors(const eps_sensor_packet_t *sensors) {
    if (sensors == NULL) {
        return;
    }

    pthread_mutex_lock(&manager_lock);
    if (sensors->timestamp_us >= state.latest_sensors.timestamp_us) {
        state.latest_sensors = *sensors;
        state.health.sensors_healthy =
            (sensors->valid_mask & EPS_SENSOR_VALID_BATTERY) != 0U;
    }
    pthread_mutex_unlock(&manager_lock);
}

void eps_manager_set_power(const eps_power_state_t *power) {
    if (power == NULL) {
        return;
    }

    pthread_mutex_lock(&manager_lock);
    if (power->timestamp_us >= state.latest_power.timestamp_us) {
        state.latest_power = *power;
    }
    pthread_mutex_unlock(&manager_lock);
}

void eps_manager_set_health(const eps_health_t *health) {
    uint32_t rejected;
    uint32_t dropped;
    uint32_t transitions;
    uint32_t rail_switches;

    if (health == NULL) {
        return;
    }

    pthread_mutex_lock(&manager_lock);
    rejected = state.health.rejected_commands;
    dropped = state.health.dropped_messages;
    transitions = state.health.mode_transitions;
    rail_switches = state.health.rail_switches;
    state.health = *health;
    state.health.rejected_commands = rejected;
    state.health.dropped_messages = dropped;
    state.health.mode_transitions = transitions;
    state.health.rail_switches = rail_switches;
    pthread_mutex_unlock(&manager_lock);
}

void eps_manager_note_rejected_command(void) {
    pthread_mutex_lock(&manager_lock);
    ++state.health.rejected_commands;
    pthread_mutex_unlock(&manager_lock);
}

void eps_manager_note_dropped_message(void) {
    pthread_mutex_lock(&manager_lock);
    ++state.health.dropped_messages;
    pthread_mutex_unlock(&manager_lock);
}

void eps_manager_note_rail_switch(void) {
    pthread_mutex_lock(&manager_lock);
    ++state.health.rail_switches;
    pthread_mutex_unlock(&manager_lock);
}

const char *eps_manager_mode_name(eps_mode_t mode) {
    static const char *const names[EPS_MODE_COUNT] = {
        "BOOT",
        "SAFE",
        "NOMINAL",
        "LOW_POWER"
    };

    return mode_is_valid(mode) ? names[mode] : "INVALID";
}

void eps_manager_get_state(eps_manager_state_t *state_out) {
    if (state_out == NULL) {
        return;
    }

    pthread_mutex_lock(&manager_lock);
    *state_out = state;
    pthread_mutex_unlock(&manager_lock);
}
