/*
 * Local EPS fault handling. Under/over-voltage, over-current,
 * over-temperature, stale sensors, solar failure, and rail faults are latched
 * for telemetry and mode fallback. The independent watchdog resets a
 * genuinely hung process after a best-effort notice to the OBC.
 */
#ifndef EPS_FAULT_MANAGER_H
#define EPS_FAULT_MANAGER_H

#include <stdint.h>

#include "communication/message.h"
#include "csp_commands.h"

typedef enum {
    EPS_FAULT_NONE = 0U,
    EPS_FAULT_UNDER_VOLTAGE = 1U << 0,
    EPS_FAULT_OVER_VOLTAGE = 1U << 1,
    EPS_FAULT_OVER_CURRENT = 1U << 2,
    EPS_FAULT_OVER_TEMPERATURE = 1U << 3,
    EPS_FAULT_SENSOR_STALE = 1U << 4,
    EPS_FAULT_SOLAR_FAILURE = 1U << 5,
    EPS_FAULT_RAIL = 1U << 6,
    EPS_FAULT_TASK_DEADLINE = 1U << 7
} eps_fault_t;

/* Every fault bit currently defined -- the single source of truth for the
   "critical" mask in eps_manager.c and the "recoverable" mask in
   housekeeping_task.c, which must otherwise be hand-kept in sync. */
#define EPS_FAULT_ALL \
    (EPS_FAULT_UNDER_VOLTAGE | EPS_FAULT_OVER_VOLTAGE | \
     EPS_FAULT_OVER_CURRENT | EPS_FAULT_OVER_TEMPERATURE | \
     EPS_FAULT_SENSOR_STALE | EPS_FAULT_SOLAR_FAILURE | \
     EPS_FAULT_RAIL | EPS_FAULT_TASK_DEADLINE)

typedef struct {
    float minimum_pack_voltage_v;
    float maximum_pack_voltage_v;
    float maximum_pack_current_a;
    float maximum_temperature_c;
    float minimum_solar_irradiance_w_m2;
    uint64_t sensor_stale_after_us;
} eps_fault_config_t;

/* Starts the independent watchdog thread. Call once from main(), before the
   FreeRTOS scheduler starts. */
void fault_management_init(void);

/* Prevents reset notices from touching CSP when EPS runs standalone. */
void fault_management_set_transport_enabled(uint8_t enabled);

/* Loads EPS-specific range and freshness thresholds before task startup. */
void fault_management_configure(const eps_fault_config_t *config);

/* Proof of life -- call periodically from a task that's actually still
   running (housekeeping). Missing this for too long is what the watchdog
   thread treats as "the board is hung". */
void fault_management_pet(void);

/* Evaluates one sensor snapshot and returns a fault bitmask. */
uint32_t fault_management_evaluate_eps(
    const eps_sensor_packet_t *sensors,
    uint64_t now_us);

/* Latches a fault for manager/telemetry consumers. */
void fault_management_report(eps_fault_t fault);

/* Returns all currently latched EPS fault bits. */
uint32_t fault_management_get_active(void);

/* Clears recoverable fault bits after their recovery criteria have been met. */
void fault_management_clear(uint32_t fault_mask);

/* Notifies the OBC (best effort) and restarts this board. Never returns on
   success. */
void fault_management_trigger_reset(reset_reason_t reason);

#endif
