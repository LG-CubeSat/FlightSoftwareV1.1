/*
v-bus API contract (docs/directory_conventions.md, docs/architecture.md).

This is the ONE interface both mediums implement:
  - platform/sim/spi/v_bus.c    -- Unix domain socket, for HW_MODE=OFF
  - platform/real/spi/spi_driver.c -- STM32 SPI peripheral, for HW_MODE=ON

Callers (shared/csp/csp_network.c) include this header by bare name only
("v_bus.h", no relative path) and never reference sim/ or real/ directly.
platform/CMakeLists.txt PUBLICly exposes this directory plus exactly one of
sim/include or real/include depending on HW_MODE, so the same call site
resolves to whichever medium is selected at configure time -- switching
mediums never requires touching a single #include anywhere above this
layer.
*/
#ifndef V_BUS_H
#define V_BUS_H

#include <stdint.h>

typedef enum {
    V_BUS_OK = 0,
    V_BUS_ERROR = -1,
    V_BUS_TIMEOUT = -2
} VBusStatus_t;

typedef struct {
    VBusStatus_t (*initialize)(int is_master);
    int (*send)(const uint8_t *data, uint16_t length);
    int (*receive)(uint8_t *buffer, uint16_t max_length);
} VBus_t;

VBus_t create_v_bus();

VBusStatus_t v_bus_initialize(int is_master);
int v_bus_send(const uint8_t *data, uint16_t length);
int v_bus_receive(uint8_t *buffer, uint16_t max_length);

#endif
