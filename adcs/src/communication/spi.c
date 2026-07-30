#include <stdio.h>
#include "spi.h"
#include "packet_parser.h"
#include "v_bus.h"

void spi_initialize(void)
{
    v_bus_initialize(0); // set to slave
}

void spi_recieve(
    const uint8_t *data,
    uint16_t length
)
{
    v_bus_receive(data, length);
    packet_parser_process(data, length);
}