/*
comms_bus API contract (docs/directory_conventions.md, docs/architecture.md).

This is the ONE interface both mediums implement:
  - platform/sim/drivers/comms_i2c.c  -- Unix domain socket, for HW_MODE=OFF
  - platform/real/drivers/comms_i2c.c -- STM32 peripheral backend, for HW_MODE=ON

Callers (shared/csp/csp_network.c) include this header by bare name only
("comms_bus.h", no relative path) and never reference sim/ or real/ directly.
platform/CMakeLists.txt PUBLICly exposes shared/interfaces plus exactly one
of sim/include or real/include depending on HW_MODE, so the same call site
resolves to whichever medium is selected at configure time -- switching
mediums never requires touching a single #include anywhere above this
layer.
*/
#ifndef COMMS_BUS_H
#define COMMS_BUS_H

#include <stdint.h>

typedef enum {
    COMMS_BUS_OK = 0,
    COMMS_BUS_ERROR = -1,
    COMMS_BUS_TIMEOUT = -2
} CommsBusStatus_t;

typedef struct {
    CommsBusStatus_t (*initialize)(uint8_t my_address, int is_master);
    int (*send)(uint8_t dest_addr, const uint8_t *data, uint16_t length);
    int (*receive)(uint8_t *sdrc_addr_out, uint8_t *buffer, uint16_t max_length);
} CommsBus_t;

CommsBus_t create_comms_bus();

CommsBusStatus_t comms_bus_initialize(uint8_t my_address, int is_master);
int comms_bus_send(uint8_t dest_addr, const uint8_t *data, uint16_t length);
int comms_bus_receive(uint8_t *src_addr_out, uint8_t *buffer, uint16_t max_length);

#endif
