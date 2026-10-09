/*
 * EPS power manager.
 *
 * The manager owns mode transitions and the latest coherent subsystem state;
 * it does not read sensors or do power math. Nominal autonomous flow is
 * BOOT -> SAFE -> NOMINAL. A critical fault forces SAFE; a low state of charge
 * drops to LOW_POWER (payload rail shed) before reaching SAFE.
 */
#ifndef EPS_MANAGER_EPS_MANAGER_H
#define EPS_MANAGER_EPS_MANAGER_H

#include <stdint.h>

#include "communication/message.h"

typedef struct {
    float low_power_state_of_charge;
    float safe_state_of_charge;
    uint64_t sensor_stale_after_us;
} eps_manager_config_t;

typedef struct {
    eps_mode_t mode;
    eps_mode_t requested_mode;
    eps_sensor_packet_t latest_sensors;
    eps_power_state_t latest_power;
    eps_health_t health;
    uint64_t mode_entered_us;
    uint8_t initialized;
} eps_manager_state_t;

/* Initializes BOOT state and requests the nominal power path. */
void eps_manager_init(void);

/* Replaces default transition thresholds before the scheduler starts. */
void eps_manager_configure(const eps_manager_config_t *config);

/* Evaluates pending requests, sensor health, faults, and autonomous
   transitions. now_us is a real wall/sim-clock timestamp, independent of
   sensor timestamps, used to judge whether that data is actually fresh. */
void eps_manager_update(uint64_t now_us);

/* Returns the active mode using the compatibility integer API. */
int eps_manager_get_mode(void);

/* Records a requested mode; update() validates the transition before
   activation. */
void eps_manager_request_mode(int mode);

/* Supplies the newest sensor packet used for health and transition
   decisions. */
void eps_manager_set_sensors(const eps_sensor_packet_t *sensors);

/* Supplies the newest derived power state for telemetry. */
void eps_manager_set_power(const eps_power_state_t *power);

/* Stores health counters and flags maintained by housekeeping and tasks. */
void eps_manager_set_health(const eps_health_t *health);

/* Records command/queue/rail events in the manager-owned health state. */
void eps_manager_note_rejected_command(void);
void eps_manager_note_dropped_message(void);
void eps_manager_note_rail_switch(void);

/* Returns a stable printable name for logs and telemetry diagnostics. */
const char *eps_manager_mode_name(eps_mode_t mode);

/* Copies one coherent manager snapshot for telemetry consumers. */
void eps_manager_get_state(eps_manager_state_t *state_out);

#endif
