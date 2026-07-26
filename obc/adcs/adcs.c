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
    Packet packet = packet_create(
        DEVICE_ADCS, 
        1, 
        CMD_POINT_TO_SUN
    );

    spi_transfer(
        (uint8_t *)&packet,
        NULL,
        sizeof(Packet)
    );
}
