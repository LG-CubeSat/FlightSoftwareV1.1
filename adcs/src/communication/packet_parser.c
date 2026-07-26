#include <stdio.h>
#include "packet_parser.h"
#include "../../../shared/protocol/packet.h"

void packet_parser_initialize(void)
{
    printf("[PARSER] Initialized.\n");
}

void packet_parser_process(
    const uint8_t *data,
    uint16_t length
)
{
    if (length < 6) {
        printf("[PARSER] Error: Packet too short.\n");
        return;
    }

    if (data[0] != PACKET_SYNC) {
        printf("[PARSER] Error: Invlaid sync byte. \n");
        return;
    }
    printf("[PARSER] Valid packet recieved\n");
    
    uint8_t destination = data[1];
    uint8_t seqeunce = data[2];
    uint8_t command = data[3];
    uint8_t payload_length = data[4];
    uint8_t crc = data[5];

    printf("    Destination: 0x%02X\n", destination);
    printf("    Sequence:    %d\n", seqeunce);
    printf("    Command:     0x%2X\n", command);
    printf("    Payload:     %d bytes\n", payload_length);
    printf("    CRC:         0x%02X\n", crc);
}