/* Hardware-agnostic contract for the EPS battery pack monitor. */
#ifndef SHARED_INTERFACES_BATTERY_H
#define SHARED_INTERFACES_BATTERY_H

#include <stdint.h>

typedef enum {
    BATTERY_OK = 0,
    BATTERY_ERROR = -1,
    BATTERY_NOT_READY = -2
} battery_status_t;

typedef struct {
    uint64_t timestamp_us;
    float pack_voltage_v;
    float pack_current_a;   /* positive = discharge, negative = charge */
    float temperature_c;
    float state_of_charge;  /* 0.0 (empty) .. 1.0 (full) */
} battery_sample_t;

battery_status_t battery_initialize(void);
battery_status_t battery_read(battery_sample_t *sample);

#endif
