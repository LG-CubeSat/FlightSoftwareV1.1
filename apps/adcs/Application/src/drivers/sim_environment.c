#include "platform/adcs_environment.h"
#include "simulation/adcs_simulator.h"
void adcs_environment_step(float dt_s) { (void)adcs_simulator_step(dt_s); }
uint64_t adcs_monotonic_time_us(void) { return adcs_simulator_get_time_us(); }
uint64_t adcs_unix_time_us(void) { return adcs_simulator_get_unix_time_us(); }
adcs_result_t adcs_set_unix_time_us(uint64_t time) { return adcs_simulator_set_unix_time(time); }
adcs_result_t adcs_get_orbit(adcs_orbit_state_t *orbit) { return adcs_simulator_get_orbit(orbit); }
adcs_result_t adcs_inject_simulator_faults(uint32_t mask) {
    adcs_simulator_set_faults(mask);
    return ADCS_RESULT_OK;
}
