/*
Real SPI backend for v-bus (see platform/include/v_bus.h). Implements the
exact same VBus_t contract as platform/sim/spi/v_bus.c so that code above
this layer (shared/csp/csp_network.c) never needs to know which medium is
selected -- only platform/CMakeLists.txt's HW_MODE switch decides which of
the two .c files gets compiled.

No STM32 HAL is vendored in this repo yet, and there is no ARM
cross-compilation toolchain wired into the build (see docs "Next Steps").
Define STM32_HAL_AVAILABLE (from a future toolchain/CMake setup once the
HAL is vendored under platform/real/include/) to compile the real
implementation below; until then this compiles to an honest stub that
returns V_BUS_ERROR, so `-DHW_MODE=ON` configures and builds cleanly today
instead of failing outright, without pretending to drive real hardware.
*/
#include "comms_bus.h"

#include <stdio.h>

#if defined(STM32_HAL_AVAILABLE)

#include "stm32xxxx_hal.h" /* vendored HAL header, not present yet */

#define VBUS_SPI_TIMEOUT_MS 1000

static SPI_HandleTypeDef hspi_vbus;

VBusStatus_t v_bus_initialize(int is_master)
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
        return V_BUS_ERROR;
    }

    return V_BUS_OK;
}

int v_bus_send(const uint8_t *data, uint16_t length)
{
    HAL_StatusTypeDef status = HAL_SPI_Transmit(&hspi_vbus, (uint8_t *)data, length, VBUS_SPI_TIMEOUT_MS);
    if (status == HAL_TIMEOUT) {
        return -2; /* matches V_BUS_TIMEOUT-style signaling used by v_bus_receive below */
    }
    return (status == HAL_OK) ? (int)length : -1;
}

int v_bus_receive(uint8_t *buffer, uint16_t max_length)
{
    HAL_StatusTypeDef status = HAL_SPI_Receive(&hspi_vbus, buffer, max_length, VBUS_SPI_TIMEOUT_MS);
    if (status == HAL_TIMEOUT) {
        return -2;
    }
    return (status == HAL_OK) ? (int)max_length : -1;
}

#else /* !STM32_HAL_AVAILABLE */

VBusStatus_t v_bus_initialize(int is_master)
{
    (void)is_master;
    fprintf(stderr,
        "[SPI DRIVER] real hardware backend not yet implemented "
        "(no STM32 HAL vendored, no cross-toolchain configured). "
        "See docs/ for the abstraction this needs to satisfy.\n");
    fflush(stderr);
    return V_BUS_ERROR;
}

int v_bus_send(const uint8_t *data, uint16_t length)
{
    (void)data;
    (void)length;
    return -1;
}

int v_bus_receive(uint8_t *buffer, uint16_t max_length)
{
    (void)buffer;
    (void)max_length;
    return -1;
}

#endif /* STM32_HAL_AVAILABLE */

VBus_t create_v_bus(void)
{
    VBus_t v_bus;
    v_bus.initialize = &v_bus_initialize;
    v_bus.send = &v_bus_send;
    v_bus.receive = &v_bus_receive;

    return v_bus;
}
