#include "utils/power_calculations.h"

#include <math.h>

float eps_instant_power_w(float voltage_v, float current_a) {
    return voltage_v * current_a;
}

float eps_peak_power_w(const float *samples, size_t count) {
    float peak = 0.0F;

    if (samples == NULL) {
        return 0.0F;
    }
    for (size_t index = 0U; index < count; ++index) {
        if (!isfinite(samples[index])) {
            continue;
        }
        if (fabsf(samples[index]) > fabsf(peak)) {
            peak = samples[index];
        }
    }
    return peak;
}

float eps_loss_power_w(float input_power_w, float output_power_w) {
    float loss = input_power_w - output_power_w;

    return loss > 0.0F ? loss : 0.0F;
}

float eps_state_of_charge(
    float pack_voltage_v,
    float cell_empty_v,
    float cell_full_v,
    uint8_t cell_count) {
    float minimum_pack_v;
    float maximum_pack_v;
    float state_of_charge;

    if (cell_count == 0U || cell_full_v <= cell_empty_v) {
        return 0.0F;
    }

    minimum_pack_v = cell_empty_v * (float)cell_count;
    maximum_pack_v = cell_full_v * (float)cell_count;
    state_of_charge =
        (pack_voltage_v - minimum_pack_v) / (maximum_pack_v - minimum_pack_v);

    if (state_of_charge < 0.0F) {
        return 0.0F;
    }
    if (state_of_charge > 1.0F) {
        return 1.0F;
    }
    return state_of_charge;
}

float eps_battery_efficiency(float discharge_energy_wh, float charge_energy_wh) {
    float efficiency;

    if (charge_energy_wh <= 0.0F) {
        return 0.0F;
    }
    efficiency = discharge_energy_wh / charge_energy_wh;
    if (efficiency < 0.0F) {
        return 0.0F;
    }
    if (efficiency > 1.0F) {
        return 1.0F;
    }
    return efficiency;
}

void eps_energy_accumulator_init(eps_energy_accumulator_t *accumulator) {
    if (accumulator == NULL) {
        return;
    }
    accumulator->accumulated_energy_wh = 0.0F;
    accumulator->elapsed_time_s = 0.0F;
    accumulator->last_timestamp_us = 0U;
    accumulator->initialized = 0U;
}

void eps_energy_accumulator_update(
    eps_energy_accumulator_t *accumulator,
    float instant_power_w,
    uint64_t timestamp_us) {
    float dt_s;

    if (accumulator == NULL) {
        return;
    }
    if (accumulator->initialized == 0U ||
        timestamp_us <= accumulator->last_timestamp_us) {
        accumulator->last_timestamp_us = timestamp_us;
        accumulator->initialized = 1U;
        return;
    }

    dt_s = (float)(timestamp_us - accumulator->last_timestamp_us) / 1000000.0F;
    accumulator->accumulated_energy_wh += instant_power_w * dt_s / 3600.0F;
    accumulator->elapsed_time_s += dt_s;
    accumulator->last_timestamp_us = timestamp_us;
}

float eps_energy_accumulator_energy_wh(
    const eps_energy_accumulator_t *accumulator) {
    if (accumulator == NULL) {
        return 0.0F;
    }
    return accumulator->accumulated_energy_wh;
}

float eps_energy_accumulator_average_power_w(
    const eps_energy_accumulator_t *accumulator) {
    if (accumulator == NULL || accumulator->elapsed_time_s <= 0.0F) {
        return 0.0F;
    }
    return accumulator->accumulated_energy_wh * 3600.0F /
           accumulator->elapsed_time_s;
}
