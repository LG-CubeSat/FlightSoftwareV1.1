#include <stdio.h>
#include "spi.h"
#include "packet_parser.h"

void spi_initialize(void)
{
    printf("[SPI] Initialized.\n");
}

void spi_recieve(
    const uint8_t *data,
    uint16_t length
)
{
    printf("[SPI] Received %d bytes.\n", length);
    packet_parser_process(data, length);
}