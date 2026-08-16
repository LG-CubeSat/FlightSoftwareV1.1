#ifndef FRAME_H
#define FRAME_H

#include <stdint.h>

#define MAX_FRAME_PAYLOAD 128

/* 
this is the framing used for the I2C bus
must be parsed when actually sent
*/
typedef struct
{
    uint8_t dest_addr;
    uint8_t src_addr;
    uint16_t length;
    uint8_t payload[MAX_FRAME_PAYLOAD];
} Frame;

#endif