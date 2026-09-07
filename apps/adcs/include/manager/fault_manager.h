/*
Local fault handling for ADCS: a hardware-watchdog-style reset for a
genuinely hung board, and an out-of-bounds check for commanded values.
Both funnel into the same reset path, which notifies the OBC (best
effort) before restarting -- see docs discussion: the board decides
and acts on its own, the OBC only tracks and decides on a shutdown.
*/
#ifndef ADCS_FAULT_MANAGER_H
#define ADCS_FAULT_MANAGER_H

#include <stdint.h>
#include "csp_commands.h"

/* Starts the independent watchdog thread. Call once from main(),
   before the FreeRTOS scheduler starts. */
void fault_management_init(void);

/* Proof of life -- call periodically from a task that's actually
   still running (housekeeping). Missing this for too long is what
   the watchdog thread treats as "the board is hung". */
void fault_management_pet(void);

/* Returns 1 if `position` is outside the allowed range. */
int fault_management_check_bounds(int32_t position);

/* Notifies the OBC (best effort) and restarts this board. Never
   returns on success. */
void fault_management_trigger_reset(reset_reason_t reason);

#endif // ADCS_FAULT_MANAGER_H
