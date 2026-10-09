/*
 * Deterministic EPS plant simulation.
 *
 * This is the software-in-the-loop stand-in for the battery pack, solar
 * array, and switchable rails. The sim drivers in
 * src/drivers/sim_eps_drivers.c map the shared hardware contracts onto this
 * simulator, exactly the way src/drivers/sim_adcs_drivers.c maps the ADCS
 * contracts onto the ADCS simulator. No function here uses "_mock" in its
 * name: these are the simulation implementations of the hardware boundary.
 */
#ifndef EPS_SIMULATION_EPS_SIMULATOR_H
#define EPS_SIMULATION_EPS_SIMULATOR_H

#include <stdint.h>

#include "communication/message.h"
#include "power_rail.h"

/* Injected fault bits, used to force the fault manager down its paths. */
typedef enum {
    EPS_SIM_FAULT_NONE = 0U,
    EPS_SIM_FAULT_LOW_VOLTAGE = 1U << 0,
    EPS_SIM_FAULT_OVER_VOLTAGE = 1U << 1,
    EPS_SIM_FAULT_OVER_CURRENT = 1U << 2,
    EPS_SIM_FAULT_OVER_TEMPERATURE = 1U << 3,
    EPS_SIM_FAULT_SOLAR_FAILURE = 1U << 4,
    EPS_SIM_FAULT_RAIL = 1U << 5
} eps_simulator_fault_t;

typedef struct {
    float initial_state_of_charge;
    float solar_irradiance_w_m2;
} eps_simulator_config_t;

typedef struct {
    uint64_t time_us;
    uint64_t unix_time_us;
    float pack_voltage_v;
    float pack_current_a;
    float battery_temperature_c;
    float state_of_charge;
    float solar_voltage_v;
    float solar_current_a;
    float solar_irradiance_w_m2;
    uint8_t illuminated;
    float rail_voltage_v[POWER_RAIL_COUNT];
    float rail_current_a[POWER_RAIL_COUNT];
    uint8_t rail_enabled[POWER_RAIL_COUNT];
    uint8_t rail_faulted[POWER_RAIL_COUNT];
    uint32_t fault_mask;
} eps_simulator_truth_t;

eps_result_t eps_simulator_init(const eps_simulator_config_t *config);
void eps_simulator_step(float dt_s);
eps_result_t eps_simulator_get_truth(eps_simulator_truth_t *out);

uint64_t eps_simulator_get_time_us(void);
uint64_t eps_simulator_get_unix_time_us(void);
eps_result_t eps_simulator_set_unix_time(uint64_t unix_time_us);

eps_result_t eps_simulator_set_rail(uint8_t rail, uint8_t enabled);
eps_result_t eps_simulator_get_rail(uint8_t rail, uint8_t *enabled_out);
eps_result_t eps_simulator_set_faults(uint32_t fault_mask);

#endif
