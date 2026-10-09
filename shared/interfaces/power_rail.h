/*
 * Hardware-agnostic contract for the EPS switchable power rails.
 *
 * The EPS owns the commandable load switches for the rest of the stack, so
 * this is both a sensor contract (read rail voltage/current) and an actuator
 * contract (enable/disable a rail). A rail that is off still reports zero
 * voltage/current, matching a real load switch.
 */
#ifndef SHARED_INTERFACES_POWER_RAIL_H
#define SHARED_INTERFACES_POWER_RAIL_H

#include <stdint.h>

#define POWER_RAIL_COUNT 3U

typedef enum {
    POWER_RAIL_3V3 = 0,   /* OBC / logic rail */
    POWER_RAIL_5V = 1,    /* shared 5V rail */
    POWER_RAIL_PAYLOAD = 2 /* switched payload rail (camera/radio) */
} power_rail_id_t;

typedef enum {
    POWER_RAIL_OK = 0,
    POWER_RAIL_ERROR = -1,
    POWER_RAIL_NOT_READY = -2
} power_rail_status_t;

typedef struct {
    uint64_t timestamp_us;
    float voltage_v[POWER_RAIL_COUNT];
    float current_a[POWER_RAIL_COUNT];
    uint8_t enabled[POWER_RAIL_COUNT];
    uint8_t faulted[POWER_RAIL_COUNT];
} power_rail_sample_t;

power_rail_status_t power_rail_initialize(void);
power_rail_status_t power_rail_read(power_rail_sample_t *sample);
power_rail_status_t power_rail_set_enabled(power_rail_id_t rail, uint8_t enabled);
power_rail_status_t power_rail_get_enabled(power_rail_id_t rail, uint8_t *enabled_out);

#endif
