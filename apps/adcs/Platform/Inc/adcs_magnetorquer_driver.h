#ifndef ADCS_MAGNATORQUER_DRIVER_H
#define ADCS_MAGNATORQUER_DRIVER_H

#include <stdint.h>
#include "magnetorquer.h"

#ifdef __cplusplus
extern "C" {
#endif

// set to 1 to run an automated bench test sequence (Forward -> Coast -> Reverse -> 0ff) at boot
#ifndef ADCS_TEST_MAGNETORQUER_AT_BOOT
#define ADCS_TEST_MAGNETORQUER_AT_BOOT 1
#endif

magnetorquer_status_t magnetorquer_initialize(void);
magnetorquer_status_t magnetorquer_set(const magnetorquer_command_t *command);
magnetorquer_status_t magnetorquer_disable(void);
magnetorquer_status_t magnetorquer_get_last_command(magnetorquer_command_t *command);

uint8_t magnetorquer_is_fault_active(void);
void magnetorquer_test_sequence(void);

#ifdef __cplusplus
}
#endif

#endif