#include <stdio.h>
#include "packet_parser.h"
#include "../../../shared/protocol/packet.h"
#include "../commands/command_queue.h"
#include "../commands/commands.h"

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
    
    CommandMessage command;

    command.destination = data[1];
    command.sequence = data[2];
    command.command = data[3];
    command.length = data[4];

    printf("[PARSER] Valid command received.\n");
    
    if (!command_queue_push(command)) {
        printf("[PARSER] ERROR: Could not queue command.\n");

        return;
    }

    printf("[PARSER] Command added to queue.\n");
}