/*
Real backend for comms_bus (see shared/interfaces/comms_bus.h). Implements
the exact same CommsBus_t contract as platform/sim/drivers/comms_i2c.c so
that code above this layer (shared/csp/csp_network.c) never needs to know
which medium is selected -- only platform/CMakeLists.txt's HW_MODE switch
decides which of the two .c files gets compiled.

NOTE: the peripheral logic below is still SPI HAL calls (HAL_SPI_Init,
SPI1, SPI_MODE_MASTER, ...), left over from before the I2C-only bus
decision (see docs/roadmap.md). Swapping this for a real I2C HAL sequence
is its own task (roadmap.md 1.6-1.8 for the SIM-side design, Phase 6 for
real hardware bring-up) -- renaming these calls without implementing real
I2C init semantics would just be a label change, not a fix, since I2C
doesn't have fields like CLKPolarity/NSS that SPI's config struct sets.

No STM32 HAL is vendored in this repo yet, and there is no ARM
cross-compilation toolchain wired into the build (see docs "Next Steps").
Define STM32_HAL_AVAILABLE (from a future toolchain/CMake setup once the
HAL is vendored under platform/real/include/) to compile the implementation
below; until then this compiles to an honest stub that returns
COMMS_BUS_ERROR, so `-DHW_MODE=ON` configures and builds cleanly today
instead of failing outright, without pretending to drive real hardware.
*/
#include "comms_bus.h"

#include <stdio.h>

#if defined(STM32_HAL_AVAILABLE)

#include "stm32xxxx_hal.h" /* vendored HAL header, not present yet */

#define VBUS_SPI_TIMEOUT_MS 1000

static SPI_HandleTypeDef hspi_vbus;

CommsBusStatus_t comms_bus_initialize(int is_master)
{
    hspi_vbus.Instance = SPI1; /* adjust to the SPI peripheral actually wired to the other board */
    hspi_vbus.Init.Mode = is_master ? SPI_MODE_MASTER : SPI_MODE_SLAVE;
    hspi_vbus.Init.Direction = SPI_DIRECTION_2LINES;
    hspi_vbus.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi_vbus.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi_vbus.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi_vbus.Init.NSS = SPI_NSS_SOFT;
    hspi_vbus.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
    hspi_vbus.Init.FirstBit = SPI_FIRSTBIT_MSB;

    if (HAL_SPI_Init(&hspi_vbus) != HAL_OK) {
        return COMMS_BUS_ERROR;
    }

    return COMMS_BUS_OK;
}

int comms_bus_send(const uint8_t *data, uint16_t length)
{
    HAL_StatusTypeDef status = HAL_SPI_Transmit(&hspi_vbus, (uint8_t *)data, length, VBUS_SPI_TIMEOUT_MS);
    if (status == HAL_TIMEOUT) {
        return -2; /* matches COMMS_BUS_TIMEOUT-style signaling used by comms_bus_receive below */
    }
    return (status == HAL_OK) ? (int)length : -1;
}

int comms_bus_receive(uint8_t *buffer, uint16_t max_length)
{
    HAL_StatusTypeDef status = HAL_SPI_Receive(&hspi_vbus, buffer, max_length, VBUS_SPI_TIMEOUT_MS);
    if (status == HAL_TIMEOUT) {
        return -2;
    }
    return (status == HAL_OK) ? (int)max_length : -1;
}

#else /* !STM32_HAL_AVAILABLE */

CommsBusStatus_t comms_bus_initialize(int is_master)
{
    (void)is_master;
    fprintf(stderr,
        "[COMMS BUS] real hardware backend not yet implemented "
        "(no STM32 HAL vendored, no cross-toolchain configured). "
        "See docs/ for the abstraction this needs to satisfy.\n");
    fflush(stderr);
    return COMMS_BUS_ERROR;
}

int comms_bus_send(const uint8_t *data, uint16_t length)
{
    (void)data;
    (void)length;
    return -1;
}

int comms_bus_receive(uint8_t *buffer, uint16_t max_length)
{
    (void)buffer;
    (void)max_length;
    return -1;
}

#endif /* STM32_HAL_AVAILABLE */

CommsBus_t create_comms_bus(void)
{
    CommsBus_t bus;
    bus.initialize = &comms_bus_initialize;
    bus.send = &comms_bus_send;
    bus.receive = &comms_bus_receive;

    return bus;
}
