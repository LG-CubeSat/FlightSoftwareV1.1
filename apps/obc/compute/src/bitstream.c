#include "bitstream.h"

void bitwriter_init(bitwriter_t *bw, uint8_t *buf, size_t cap)
{
    bw->buf = buf;
    bw->cap = cap;
    bw->bit_pos = 0;
}

static int bitwriter_put_bit(bitwriter_t *bw, int bit)
{
    size_t byte_index = bw-> bit_pos / 8;
    if (byte_index >= bw->cap) return -1;

    int bit_offset = bw->bit_pos % 8;
    if (bit_offset == 0) {
        bw->buf[byte_index] = 0; // first touch of this byte -- clear it
    }
    if (bit) {
        bw->buf[byte_index] |= (uint8_t)(1u << (7 - bit_offset));
    }

    bw->bit_pos++;
    return 0;
}

int bitwriter_put_bits(bitwriter_t *bw, uint32_t value, int nbits)
{
    for (int i = nbits - 1; i >= 0; i--) {
        if (bitwriter_put_bit(bw, (value >> i) & 1u) != 0) return -1;
    }
    return 0;
}

int bitwriter_put_unary(bitwriter_t *bw, uint32_t q)
{
    for (uint32_t i = 0; i < q; i++) {
        if (bitwriter_put_bit(bw, 1) != 0) return -1;
    }
    return bitwriter_put_bit(bw, 0);
}

size_t bitwriter_flush(bitwriter_t *bw)
{
    if (bw->bit_pos % 8 != 0) {
        bw->bit_pos = ((bw->bit_pos / 8) + 1) * 8;
    }
    return bw->bit_pos / 8;
}

void bitreader_init(bitreader_t *br, const uint8_t *buf, size_t len)
{
    br->buf = buf;
    br->len = len;
    br->bit_pos = 0;
}

static int bitreader_get_bit(bitreader_t *br)
{
    size_t byte_index = br->bit_pos / 8;
    if (byte_index >= br->len) return 0; // past end -- shouldn't happen against our own encoder's output

    int bit_offset = br->bit_pos % 8;
    int bit = (br->buf[byte_index] >> (7 - bit_offset)) & 1;
    br->bit_pos++;
    return bit;
}

uint32_t bitreader_get_bits(bitreader_t *br, int nbits)
{
    uint32_t value = 0;
    for (int i = 0; i < nbits; i++) {
        value = (value << 1) | (uint32_t)bitreader_get_bit(br);
    }
    return value;
}

uint32_t bitreader_get_unary(bitreader_t *br)
{
    uint32_t q = 0;
    while (bitreader_get_bit(br) == 1) {
        q++;
    }
    return q;
}