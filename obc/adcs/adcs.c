#include "adcs.h"
#include "../communication/spi.h"

#include "../../shared/protocol/commands.h"
#include "../../shared/protocol/packet.h"

#include <stdio.h>

void adcs_initialize(void)
{
    printf("ADCS Initalized");
}

void adcs_point_to_sun(void)
{
    Packet packet;
    packet.sync = PACKET_SYNC;
    packet.command = CMD_POINT_TO_SUN;
    packet.length = 0;
    packet.crc = 0;

    spi_transfer(
        (uint8_t *)&packet,
        NULL,
        sizeof(Packet)
    );
}
