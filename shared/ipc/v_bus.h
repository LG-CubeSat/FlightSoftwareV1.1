#ifndef V_BUS_H
#define V_BUS_H

#include <stdint.h>

typedef enum {
    V_BUS_OK = 0,
    V_BUS_ERROR = -1,
    V_BUS_TIMEOUT = -2
} VBusStatus_t;

VBusStatus_t v_bus_initialize(int is_master);

int v_bus_send(const uint8_t *data, uint16_t length);
int v_bus_receive(uint8_t *buffer, uint16_t max_length);

#endif