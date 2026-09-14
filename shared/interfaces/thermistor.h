/* Hardware-agnostic contract for the ADCS board-temperature sensor. */
#ifndef SHARED_INTERFACES_THERMISTOR_H
#define SHARED_INTERFACES_THERMISTOR_H

#include <stdint.h>

typedef enum {
    THERMISTOR_OK = 0,
    THERMISTOR_ERROR = -1,
    THERMISTOR_NOT_READY = -2
} thermistor_status_t;

typedef struct {
    uint64_t timestamp_us;
    float temperature_c;
} thermistor_sample_t;

thermistor_status_t thermistor_initialize(void);
thermistor_status_t thermistor_read(thermistor_sample_t *sample);

#endif
