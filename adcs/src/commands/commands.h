#ifndef COMMAND_H
#define COMMAND_H

#include <stdint.h>

// internal commands (not from obc -> mcu) don't need sync or crc as its checked by spi parser
typedef struct
{
    uint8_t destination;
    uint8_t sequence;
    uint8_t command;
    uint8_t length;
} CommandMessage;

#endif
