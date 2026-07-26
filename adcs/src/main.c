#include <stdio.h>
#include "communication/packet_parser.h"
#include "communication/spi.h"
#include "commands/command_queue.h"

int main(void)
{
    printf("LG Cubesat ADCS starting...\n");

    printf("Initializing Hardware...\n");

    spi_initialize();

    packet_parser_initialize();

    command_queue_initialize();

    printf("Starting RTOS...\n");

    printf("ADCS ready.\n");

    uint8_t test_packet[] = 
    {
        0xAA, // sync
        0x02, // Destination: ADCS
        0x01, // Sequence
        0x01, // Command: POINT_TO_SUN
        0x00, // Payload length
        0x00, // CRC (fake for now)
    };

    printf("\nSimulating SPI reception...\n");
    
    spi_recieve(
        test_packet,
        sizeof(test_packet)
    );

    return 0;
}