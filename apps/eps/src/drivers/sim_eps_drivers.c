/* Simulation implementations of the shared EPS hardware contracts. */
#include "battery.h"
#include "power_rail.h"
#include "solar_array.h"

#include <string.h>

#include "simulation/eps_simulator.h"

static uint8_t simulator_ready;

static uint8_t simulator_is_ready(void) {
    eps_simulator_truth_t truth;

    if (simulator_ready == 0U ||
        eps_simulator_get_truth(&truth) != EPS_RESULT_OK) {
        return 0U;
    }
    return 1U;
}

battery_status_t battery_initialize(void) {
    eps_simulator_truth_t truth;

    if (eps_simulator_get_truth(&truth) != EPS_RESULT_OK) {
        simulator_ready = 0U;
        return BATTERY_ERROR;
    }
    simulator_ready = 1U;
    return BATTERY_OK;
}

battery_status_t battery_read(battery_sample_t *sample) {
    eps_simulator_truth_t truth;

    if (sample == NULL) {
        return BATTERY_ERROR;
    }
    if (!simulator_is_ready()) {
        return BATTERY_NOT_READY;
    }
    if (eps_simulator_get_truth(&truth) != EPS_RESULT_OK) {
        return BATTERY_ERROR;
    }

    sample->timestamp_us = truth.time_us;
    sample->pack_voltage_v = truth.pack_voltage_v;
    sample->pack_current_a = truth.pack_current_a;
    sample->temperature_c = truth.battery_temperature_c;
    sample->state_of_charge = truth.state_of_charge;
    return BATTERY_OK;
}

solar_array_status_t solar_array_initialize(void) {
    eps_simulator_truth_t truth;

    if (eps_simulator_get_truth(&truth) != EPS_RESULT_OK) {
        simulator_ready = 0U;
        return SOLAR_ARRAY_ERROR;
    }
    simulator_ready = 1U;
    return SOLAR_ARRAY_OK;
}

solar_array_status_t solar_array_read(solar_array_sample_t *sample) {
    eps_simulator_truth_t truth;

    if (sample == NULL) {
        return SOLAR_ARRAY_ERROR;
    }
    if (!simulator_is_ready()) {
        return SOLAR_ARRAY_NOT_READY;
    }
    if (eps_simulator_get_truth(&truth) != EPS_RESULT_OK) {
        return SOLAR_ARRAY_ERROR;
    }

    sample->timestamp_us = truth.time_us;
    sample->voltage_v = truth.solar_voltage_v;
    sample->current_a = truth.solar_current_a;
    sample->irradiance_w_m2 = truth.solar_irradiance_w_m2;
    sample->illuminated = truth.illuminated;
    return SOLAR_ARRAY_OK;
}

power_rail_status_t power_rail_initialize(void) {
    eps_simulator_truth_t truth;

    if (eps_simulator_get_truth(&truth) != EPS_RESULT_OK) {
        simulator_ready = 0U;
        return POWER_RAIL_ERROR;
    }
    simulator_ready = 1U;
    return POWER_RAIL_OK;
}

power_rail_status_t power_rail_read(power_rail_sample_t *sample) {
    eps_simulator_truth_t truth;

    if (sample == NULL) {
        return POWER_RAIL_ERROR;
    }
    if (!simulator_is_ready()) {
        return POWER_RAIL_NOT_READY;
    }
    if (eps_simulator_get_truth(&truth) != EPS_RESULT_OK) {
        return POWER_RAIL_ERROR;
    }

    sample->timestamp_us = truth.time_us;
    memcpy(sample->voltage_v, truth.rail_voltage_v, sizeof(sample->voltage_v));
    memcpy(sample->current_a, truth.rail_current_a, sizeof(sample->current_a));
    memcpy(sample->enabled, truth.rail_enabled, sizeof(sample->enabled));
    memcpy(sample->faulted, truth.rail_faulted, sizeof(sample->faulted));
    return POWER_RAIL_OK;
}

power_rail_status_t power_rail_set_enabled(power_rail_id_t rail, uint8_t enabled) {
    if (eps_simulator_set_rail((uint8_t)rail, enabled) != EPS_RESULT_OK) {
        return POWER_RAIL_ERROR;
    }
    return POWER_RAIL_OK;
}

power_rail_status_t power_rail_get_enabled(
    power_rail_id_t rail,
    uint8_t *enabled_out) {
    if (eps_simulator_get_rail((uint8_t)rail, enabled_out) != EPS_RESULT_OK) {
        return POWER_RAIL_ERROR;
    }
    return POWER_RAIL_OK;
}
