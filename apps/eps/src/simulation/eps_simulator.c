#include "simulation/eps_simulator.h"

#include <pthread.h>
#include <string.h>
#include <time.h>

#define EPS_SIM_CELL_COUNT 4
#define EPS_SIM_CELL_EMPTY_V 3.0F
#define EPS_SIM_CELL_FULL_V 4.2F
#define EPS_SIM_BATTERY_CAPACITY_AH 5.0F
#define EPS_SIM_BASE_LOAD_A 0.6F
#define EPS_SIM_PAYLOAD_LOAD_A 1.2F
#define EPS_SIM_SOLAR_VOLTAGE_V 18.0F
#define EPS_SIM_SOLAR_MAX_CURRENT_A 1.2F
#define EPS_SIM_SOLAR_REFERENCE_IRRADIANCE 800.0F
#define EPS_SIM_DEFAULT_SOC 0.90F
#define EPS_SIM_DEFAULT_IRRADIANCE 800.0F

static pthread_mutex_t simulator_lock = PTHREAD_MUTEX_INITIALIZER;
static eps_simulator_truth_t truth;
static eps_simulator_config_t simulator_config;
static uint8_t initialized;

static uint8_t rail_is_valid(uint8_t rail) {
    return rail < POWER_RAIL_COUNT;
}

static float rail_nominal_voltage(uint8_t rail) {
    switch (rail) {
        case POWER_RAIL_3V3: return 3.3F;
        case POWER_RAIL_5V: return 5.0F;
        case POWER_RAIL_PAYLOAD: return 12.0F;
        default: return 0.0F;
    }
}

static float rail_nominal_current(uint8_t rail) {
    switch (rail) {
        case POWER_RAIL_3V3: return 0.30F;
        case POWER_RAIL_5V: return 0.45F;
        case POWER_RAIL_PAYLOAD: return EPS_SIM_PAYLOAD_LOAD_A;
        default: return 0.0F;
    }
}

/* Recomputes the derived truth fields from the persistent state. Caller holds
   simulator_lock. */
static void refresh_truth_locked(void) {
    float total_load_a = EPS_SIM_BASE_LOAD_A;
    float solar_charge_a;
    float net_current_a;

    /* Battery pack voltage tracks state of charge across a 4S Li-ion range. */
    truth.pack_voltage_v = EPS_SIM_CELL_COUNT *
        (EPS_SIM_CELL_EMPTY_V +
         truth.state_of_charge * (EPS_SIM_CELL_FULL_V - EPS_SIM_CELL_EMPTY_V));

    for (uint8_t rail = 0U; rail < POWER_RAIL_COUNT; ++rail) {
        if (truth.rail_enabled[rail] != 0U) {
            truth.rail_voltage_v[rail] = rail_nominal_voltage(rail);
            truth.rail_current_a[rail] = rail_nominal_current(rail);
            total_load_a += truth.rail_current_a[rail];
        } else {
            truth.rail_voltage_v[rail] = 0.0F;
            truth.rail_current_a[rail] = 0.0F;
        }
        truth.rail_faulted[rail] = 0U;
    }

    if ((truth.fault_mask & EPS_SIM_FAULT_SOLAR_FAILURE) != 0U) {
        truth.solar_irradiance_w_m2 = 0.0F;
        truth.solar_current_a = 0.0F;
        truth.solar_voltage_v = 0.0F;
        truth.illuminated = 0U;
    } else {
        truth.solar_irradiance_w_m2 = simulator_config.solar_irradiance_w_m2;
        truth.solar_voltage_v = EPS_SIM_SOLAR_VOLTAGE_V;
        truth.solar_current_a =
            EPS_SIM_SOLAR_MAX_CURRENT_A *
            (simulator_config.solar_irradiance_w_m2 /
             EPS_SIM_SOLAR_REFERENCE_IRRADIANCE);
        truth.illuminated = 1U;
    }

    solar_charge_a = truth.solar_current_a;
    net_current_a = total_load_a - solar_charge_a;
    truth.pack_current_a = net_current_a;
    truth.battery_temperature_c = 25.0F + 2.0F * net_current_a;

    if ((truth.fault_mask & EPS_SIM_FAULT_RAIL) != 0U) {
        truth.rail_faulted[POWER_RAIL_PAYLOAD] = 1U;
    }
    if ((truth.fault_mask & EPS_SIM_FAULT_LOW_VOLTAGE) != 0U) {
        truth.pack_voltage_v = 10.0F;
    }
    if ((truth.fault_mask & EPS_SIM_FAULT_OVER_VOLTAGE) != 0U) {
        truth.pack_voltage_v = 18.0F;
    }
    if ((truth.fault_mask & EPS_SIM_FAULT_OVER_CURRENT) != 0U) {
        truth.pack_current_a = 9.0F;
    }
    if ((truth.fault_mask & EPS_SIM_FAULT_OVER_TEMPERATURE) != 0U) {
        truth.battery_temperature_c = 70.0F;
    }
}

eps_result_t eps_simulator_init(const eps_simulator_config_t *config) {
    eps_simulator_config_t defaults = {
        .initial_state_of_charge = EPS_SIM_DEFAULT_SOC,
        .solar_irradiance_w_m2 = EPS_SIM_DEFAULT_IRRADIANCE
    };

    if (config == NULL) {
        config = &defaults;
    }
    if (config->initial_state_of_charge < 0.0F ||
        config->initial_state_of_charge > 1.0F ||
        config->solar_irradiance_w_m2 < 0.0F) {
        return EPS_RESULT_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&simulator_lock);
    memset(&truth, 0, sizeof(truth));
    simulator_config = *config;
    truth.time_us = 0U;
    truth.unix_time_us = 0U;
    truth.state_of_charge = config->initial_state_of_charge;
    truth.rail_enabled[POWER_RAIL_3V3] = 1U;
    truth.rail_enabled[POWER_RAIL_5V] = 1U;
    truth.rail_enabled[POWER_RAIL_PAYLOAD] = 1U;
    initialized = 1U;
    refresh_truth_locked();
    pthread_mutex_unlock(&simulator_lock);
    return EPS_RESULT_OK;
}

void eps_simulator_step(float dt_s) {
    float net_current_a;
    float dt_hours;

    if (dt_s <= 0.0F) {
        return;
    }

    pthread_mutex_lock(&simulator_lock);
    if (initialized == 0U) {
        pthread_mutex_unlock(&simulator_lock);
        return;
    }

    truth.time_us += (uint64_t)(dt_s * 1000000.0F);
    truth.unix_time_us += (uint64_t)(dt_s * 1000000.0F);

    net_current_a = truth.pack_current_a;
    dt_hours = dt_s / 3600.0F;
    /* Positive current discharges the pack. */
    truth.state_of_charge -=
        (net_current_a * dt_hours) / EPS_SIM_BATTERY_CAPACITY_AH;
    if (truth.state_of_charge < 0.0F) {
        truth.state_of_charge = 0.0F;
    } else if (truth.state_of_charge > 1.0F) {
        truth.state_of_charge = 1.0F;
    }

    refresh_truth_locked();
    pthread_mutex_unlock(&simulator_lock);
}

eps_result_t eps_simulator_get_truth(eps_simulator_truth_t *out) {
    if (out == NULL) {
        return EPS_RESULT_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&simulator_lock);
    if (initialized == 0U) {
        pthread_mutex_unlock(&simulator_lock);
        return EPS_RESULT_NOT_INITIALIZED;
    }
    *out = truth;
    pthread_mutex_unlock(&simulator_lock);
    return EPS_RESULT_OK;
}

uint64_t eps_simulator_get_time_us(void) {
    uint64_t time_us;

    pthread_mutex_lock(&simulator_lock);
    time_us = initialized != 0U ? truth.time_us : 0U;
    pthread_mutex_unlock(&simulator_lock);
    return time_us;
}

uint64_t eps_simulator_get_unix_time_us(void) {
    uint64_t unix_time_us;

    pthread_mutex_lock(&simulator_lock);
    unix_time_us = initialized != 0U ? truth.unix_time_us : 0U;
    pthread_mutex_unlock(&simulator_lock);
    return unix_time_us;
}

eps_result_t eps_simulator_set_unix_time(uint64_t unix_time_us) {
    pthread_mutex_lock(&simulator_lock);
    if (initialized == 0U) {
        pthread_mutex_unlock(&simulator_lock);
        return EPS_RESULT_NOT_INITIALIZED;
    }
    truth.unix_time_us = unix_time_us;
    pthread_mutex_unlock(&simulator_lock);
    return EPS_RESULT_OK;
}

eps_result_t eps_simulator_set_rail(uint8_t rail, uint8_t enabled) {
    if (!rail_is_valid(rail)) {
        return EPS_RESULT_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&simulator_lock);
    if (initialized == 0U) {
        pthread_mutex_unlock(&simulator_lock);
        return EPS_RESULT_NOT_INITIALIZED;
    }
    truth.rail_enabled[rail] = enabled != 0U ? 1U : 0U;
    refresh_truth_locked();
    pthread_mutex_unlock(&simulator_lock);
    return EPS_RESULT_OK;
}

eps_result_t eps_simulator_get_rail(uint8_t rail, uint8_t *enabled_out) {
    if (!rail_is_valid(rail) || enabled_out == NULL) {
        return EPS_RESULT_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&simulator_lock);
    if (initialized == 0U) {
        pthread_mutex_unlock(&simulator_lock);
        return EPS_RESULT_NOT_INITIALIZED;
    }
    *enabled_out = truth.rail_enabled[rail];
    pthread_mutex_unlock(&simulator_lock);
    return EPS_RESULT_OK;
}

eps_result_t eps_simulator_set_faults(uint32_t fault_mask) {
    pthread_mutex_lock(&simulator_lock);
    if (initialized == 0U) {
        pthread_mutex_unlock(&simulator_lock);
        return EPS_RESULT_NOT_INITIALIZED;
    }
    truth.fault_mask = fault_mask;
    refresh_truth_locked();
    pthread_mutex_unlock(&simulator_lock);
    return EPS_RESULT_OK;
}
