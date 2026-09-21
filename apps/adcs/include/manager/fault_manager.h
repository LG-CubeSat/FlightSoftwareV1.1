/*
 * Local ADCS fault handling. Sensor, estimate, rate, and actuator faults are
 * latched for telemetry and mode fallback. The independent watchdog
 * resets a genuinely hung process after a best-effort notice to the OBC.
 */
#ifndef ADCS_FAULT_MANAGER_H
#define ADCS_FAULT_MANAGER_H

#include <stdint.h>
#include "csp_commands.h"
#include "communication/message.h"

typedef enum {
    ADCS_FAULT_NONE = 0U,
    ADCS_FAULT_SENSOR_STALE = 1U << 0,
    ADCS_FAULT_SENSOR_RANGE = 1U << 1,
    ADCS_FAULT_ATTITUDE_INVALID = 1U << 2,
    ADCS_FAULT_EXCESSIVE_RATE = 1U << 3,
    ADCS_FAULT_ACTUATOR = 1U << 4,
    ADCS_FAULT_TASK_DEADLINE = 1U << 5
} adcs_fault_t;

/* Every fault bit currently defined -- the single source of truth for the
   "critical" mask in adcs_manager.c and the "recoverable" mask in
   housekeeping_task.c, which must otherwise be hand-kept in sync. */
#define ADCS_FAULT_ALL \
    (ADCS_FAULT_SENSOR_STALE | ADCS_FAULT_SENSOR_RANGE | \
     ADCS_FAULT_ATTITUDE_INVALID | ADCS_FAULT_EXCESSIVE_RATE | \
     ADCS_FAULT_ACTUATOR | ADCS_FAULT_TASK_DEADLINE)

typedef struct {
    float maximum_rate_rad_s;
    float minimum_magnetic_field_t;
    float maximum_magnetic_field_t;
    uint64_t sensor_stale_after_us;
    uint64_t estimate_stale_after_us;
} adcs_fault_config_t;

/* Starts the independent watchdog thread. Call once from main(),
   before the FreeRTOS scheduler starts. */
void fault_management_init(void);

/* Prevents reset notices from touching CSP when ADCS runs standalone. */
void fault_management_set_transport_enabled(uint8_t enabled);

/* Loads ADCS-specific range and freshness thresholds before task startup. */
void fault_management_configure(const adcs_fault_config_t *config);

/* Proof of life -- call periodically from a task that's actually
   still running (housekeeping). Missing this for too long is what
   the watchdog thread treats as "the board is hung". */
void fault_management_pet(void);

/* Returns 1 only for a materially corrupted estimator state. */
int fault_management_check_estimator_bounds(
    const adcs_attitude_state_t *attitude);

/* Evaluates sensor, estimator, and control outputs and returns a fault bitmask. */
uint32_t fault_management_evaluate_adcs(
    const adcs_sensor_packet_t *sensors,
    const adcs_attitude_state_t *attitude,
    const adcs_control_output_t *control,
    uint64_t now_us);

/* Latches a fault for manager/telemetry consumers and forces SAFE if required. */
void fault_management_report(adcs_fault_t fault);

/* Returns all currently latched ADCS fault bits. */
uint32_t fault_management_get_active(void);

/* Clears recoverable fault bits after their recovery criteria have been met. */
void fault_management_clear(uint32_t fault_mask);

/* Notifies the OBC (best effort) and restarts this board. Never
   returns on success. */
void fault_management_trigger_reset(reset_reason_t reason);

#endif
