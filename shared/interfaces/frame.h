#ifndef FRAME_H
#define FRAME_H

#include <stdint.h>
#include <stddef.h> // size_t

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

/*
Wire format: [dest_addr:1][src_addr:1][length:2, network byte order][payload:length].
Medium-agnostic -- shared between platform/sim and platform/real so both
backends encode/decode frames identically regardless of the physical
transport underneath.
*/
int frame_serialize(const Frame *frame, uint8_t *out_buf, size_t out_buf_size);
int frame_deserialize(const uint8_t *wire_buf, int wire_len, Frame *frame_out);

#endif