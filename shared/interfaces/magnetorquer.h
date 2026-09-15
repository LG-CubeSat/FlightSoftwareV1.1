/* Hardware-agnostic contract for three-axis magnetic torque rods/coils. */
#ifndef SHARED_INTERFACES_MAGNETORQUER_H
#define SHARED_INTERFACES_MAGNETORQUER_H

#include <stdint.h>

typedef enum {
    MAGNETORQUER_OK = 0,
    MAGNETORQUER_ERROR = -1,
    MAGNETORQUER_NOT_READY = -2
} magnetorquer_status_t;

typedef struct {
    float dipole_a_m2[3];
    uint8_t enabled;
} magnetorquer_command_t;

magnetorquer_status_t magnetorquer_initialize(void);
magnetorquer_status_t magnetorquer_set(const magnetorquer_command_t *command);
magnetorquer_status_t magnetorquer_disable(void);
magnetorquer_status_t magnetorquer_get_last_command(
    magnetorquer_command_t *command);

#endif
