#ifndef ADCS_ENVIRONMENT_H
#define ADCS_ENVIRONMENT_H
#include "communication/message.h"
/* Task-context services supplied by either the simulator or STM32 platform. */
void adcs_environment_step(float dt_s);
uint64_t adcs_monotonic_time_us(void);
uint64_t adcs_unix_time_us(void);
adcs_result_t adcs_set_unix_time_us(uint64_t unix_time_us);
adcs_result_t adcs_get_orbit(adcs_orbit_state_t *orbit);
adcs_result_t adcs_inject_simulator_faults(uint32_t mask);
#endif
