#ifndef SPI_H
#define SPI_H

#include <stdint.h>

void spi_initailize(void);

int spi_transfer(
    const uint8_t *tx_data,
    uint8_t *rx_data,
    uint16_t length
);

#endif