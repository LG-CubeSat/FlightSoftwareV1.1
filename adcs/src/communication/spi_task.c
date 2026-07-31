#include <stdio.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#include "spi_task.h"
#include "../commands/command_queue.h"
#include "../../../shared/ipc/v_bus.h"
#include "../../../shared/protocol/ccsds.h"
#include "../../../shared/protocol/crc.h"

static StaticTask_t xSpiTaskBuffer;
static StackType_t xSpiStack[configMINIMAL_STACK_SIZE];
// a global packet buffer so we don't overflow the stack
static CCSDS_Packet_t rx_packet;

void spi_task_run(void)
{
    // block here until a packet arrives on the virtual wire
    int bytes_received = v_bus_receive((uint8_t *)&rx_packet, sizeof(rx_packet));

    if (bytes_received > 0)
    {
        // caluclate crc over header and payload (not crc itself)
        uint16_t calculated_crc =ccsds_calculate_crc(
            (uint8_t *)&rx_packet,
            sizeof(rx_packet.header) + MAX_PAYLOAD_SIZE // notice how using this sizing clips out the crc at the end
        );

        // verify integrity
        if (calculated_crc == rx_packet.crc)
        {
            printf("[SPI] Packet Verified (CRC Match). APID: %d\n", ccsds_get_apid(rx_packet.header.id));
            
            CommandMessage cmd;
            
            cmd.destination = (uint8_t)ccsds_get_apid(rx_packet.header.id);
            cmd.sequence = (uint8_t)ccsds_swap16(rx_packet.header.sequence_control);
            cmd.command = rx_packet.payload[0];
            cmd.length = (uint8_t)ccsds_swap16(rx_packet.header.length);

            command_queue_push(cmd);

            // map to internal command soon
        } else {
            printf("[SPI] Error: CRC Mismatch! Expected 0x%04X, Got 0x%4X\n",
                rx_packet.crc, calculated_crc);
        }
    }
}

void spi_task_loop(void *pvParameters)
{
    printf("[SPI] Started Task.\n");
    for (;;) {
        spi_task_run();
    }
}

TaskHandle_t spi_task_create_static(void)
{
    return xTaskCreateStatic(
        spi_task_loop,
        "SPI_RX",
        configMINIMAL_STACK_SIZE,
        NULL,
        3,
        xSpiStack,
        &xSpiTaskBuffer
    );
}