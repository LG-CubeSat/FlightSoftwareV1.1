#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>
#include "commands.h"

#define PACKET_SYNC 0xAA

typedef enum {
    DEVICE_OBC = 0x01,
    DEVICE_ADCS = 0x02,
    DEVICE_EPS = 0x03,
    DEVICE_RADIO = 0x04,
    DEVICE_PAYLOAD = 0x05
} DeviceId;

typedef struct
{
    uint8_t sync; // signifies start point
    uint8_t dest; // who the message is meant for
    uint8_t seq; // like an id tag
    uint8_t command; // the actual command to perform
    uint8_t length; // how long is the packet in bytes
    uint8_t crc; // did this get corrupted?
} Packet;

Packet packet_create(
    DeviceId destination,
    uint8_t sequence, 
    CommandId command
);

#endif
