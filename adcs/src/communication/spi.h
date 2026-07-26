#ifndef SPI_H
#define SPI_H

#include <stdint.h>

void spi_initialize(void);

void spi_recieve(
    const uint8_t *data,
    uint16_t length
);

#endif