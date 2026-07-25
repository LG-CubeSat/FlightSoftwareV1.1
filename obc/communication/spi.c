#include "spi.h"
#include <stdio.h>

void spi_initialize(void)
{
    printf("SPI initialized...\n");
}

int spi_transfer(
    const uint8_t *tx_data,
    uint8_t *rx_data,
    uint16_t length
)
{
    printf("SPI transfered: %d bytes\n", length);
    return 0;
}