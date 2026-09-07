#ifndef RICE_CODEC_H
#define RICE_CODEC_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    SAMPLE_WIDTH_8 = 1,
    SAMPLE_WIDTH_16 = 2,
    SAMPLE_WIDTH_32 = 4
} sample_width_t;

int rice_compress(const uint8_t *in, size_t in_len, sample_width_t width, uint8_t *out, size_t out_cap, size_t *out_len);
int rice_decompress(const uint8_t *in, size_t in_len, sample_width_t width, uint8_t *out, size_t out_cap, size_t *out_len);


#endif