#include "packet.h"
#include "commands.h"

Packet packet_create(
    DeviceId destination,
    uint8_t sequence,
    CommandId command
)
{
    Packet packet;
    
    packet.sync = 0xAA;
    packet.dest = destination;
    packet.seq = sequence;
    packet.command = command;
    packet.length = 0;
    packet.crc = 0;

    return packet;
}