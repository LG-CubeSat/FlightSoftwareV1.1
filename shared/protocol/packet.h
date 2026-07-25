#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>

#define PACKET_SYNC 0xAA

typedef struct
{
    uint8_t sync; // signifies start point
    uint8_t command; // the actual command to perform
    uint8_t length; // how long is the packet in bytes
    uint8_t crc; // did this get corrupted?
} Packet;

#endif
