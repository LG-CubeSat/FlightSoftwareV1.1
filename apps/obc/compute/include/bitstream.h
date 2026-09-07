#ifndef OBC_COMPUTE_BITSTREAM_H
#define OBC_COMPUTE_BITSTREAM_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *buf;
    size_t cap; // buffer capacity in bytes
    size_t bit_pos; // next bit to writes
} bitwriter_t;

typedef struct {
    const uint8_t *buf;
    size_t len; // buffer length in bytes
    size_t bit_pos; // next bit to read
} bitreader_t;

/* reader */
void bitwriter_init(bitwriter_t *bw, uint8_t *buf, size_t cap);
int bitwriter_put_bits(bitwriter_t *bw, uint32_t value, int nbits); // low nbits of value, MSB-first
int bitwriter_put_unary(bitwriter_t *bw, uint32_t q); //q ones, then a zero
size_t bitwriter_flush(bitwriter_t *bw); // pads to a byte boundary, returns bytes used

/* writer */
void bitreader_init(bitreader_t *br, const uint8_t *buf, size_t len);
uint32_t bitreader_get_bits(bitreader_t *br, int nbits);
uint32_t bitreader_get_unary(bitreader_t *br);

#endif