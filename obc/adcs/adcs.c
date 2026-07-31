#include <stdio.h>
#include "adcs.h"

#include "../../shared/protocol/ccsds.h"
#include "../../shared/protocol/crc.h"
#include "../../shared/ipc/v_bus.h"

#include <string.h>

void adcs_initialize(void)
{
    printf("ADCS Initalized");
}

void adcs_point_to_sun(void)
{
    CCSDS_Packet_t packet;

    // setup header (Big Edian)
    packet.header.id = ccsds_swap16(APID_ADCS_CMD);
    packet.header.sequence_control = 0;
    packet.header.length = ccsds_swap16(MAX_PAYLOAD_SIZE - 1);

    memset(packet.payload, 0, MAX_PAYLOAD_SIZE);
    packet.payload[0] = 0x01; // different commands (point to sun for example)

    // calculate the crc
    packet.crc = ccsds_calculate_crc((uint8_t *)&packet, sizeof(packet.header) + MAX_PAYLOAD_SIZE);

    // send over the virtual bus
    v_bus_send((uint8_t*)&packet, sizeof(packet));
}
