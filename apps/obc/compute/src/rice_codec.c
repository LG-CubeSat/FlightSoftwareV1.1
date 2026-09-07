#include "rice_codec.h"
#include "bitstream.h"

#include <string.h>

#define RICE_BLOCK_SIZE 16
#define BLOCK_TYPE_BITS 2
#define K_FIELD_BITS 5 // stores k = 0..31 which is all we ever produice (see max k below)
#define BLOCK_TYPE_ZERO 0
#define BLOCK_TYPE_RICE 1
#define BLOCK_TYPE_VERBATIM 2

#define RICE_MAX_SAMPLES (64 * 1024) // matches current photo payload size max

typedef struct {
    uint32_t n_samples; // how many full 'width'-sized samples were block-encoded
    uint16_t tail_len; // 0..width-1 leftover bytes, stored raw right after the bitstream
    uint8_t width; // self-describing/defensive -- checked against the caller's width on decode
} rice_header_t;

static uint32_t read_sample(const uint8_t *in, size_t index, sample_width_t width)
{
    size_t off = index * (size_t)width; // adjustable offset
    uint32_t v = in[off]; // first bit

    // little endian formatting (like reading right -> left)
    if (width >= SAMPLE_WIDTH_16) v |= (uint32_t)(in[off+1] << 8); // if another byte, throw in front
    if (width >= SAMPLE_WIDTH_32) v |= ((uint32_t)(in[off+2] << 16) | (uint32_t)(in[off+3] << 24)); // if 2 more bytes, throw in front

    return v;
}

static void write_sample(uint8_t *out, size_t index, sample_width_t width, uint32_t value)
{
    size_t off = index * (size_t)width;
    out[off] = (uint8_t)value; // take first bit and put into index 'off'

    if (width >= SAMPLE_WIDTH_16) { out[off+1] = (uint8_t)(value >> 8); } // shifts second byte over and assigns to out byte two
    if (width >= SAMPLE_WIDTH_32) {
        out[off+2] = (uint8_t)(value >> 16);
        out[off+3] = (uint8_t)(value >> 24);
    }
}

/* Reinterprets a `width_bits`-wide wraparound value as signed, then
   zig-zag maps it to unsigned (0,-1,1,-2,2,... -> 0,1,2,3,4,...). Result
   always fits in exactly `width_bits` bits -- see the writeup on why. */
static uint32_t zigzag_encode(uint32_t wrapped_diff, int width_bits)
{
    uint32_t sign_bit = 1u << (width_bits - 1); // pushes bit value of 1 to left-most position (which is negative)
    int32_t signed_val = (int32_t)((wrapped_diff ^ sign_bit) - sign_bit); // flips the top bits and subtracts resulting in 32 bit signed
    return ((uint32_t)signed_val << 1) ^ (uint32_t)(signed_val >> 31); // 1. Shifts all bits by one. Then XORs, drags signal value (00...) or (11...) converting to positive.
}

/* Inverse of zigzag_encode: returns the wraparound value to add back in. */
static uint32_t zigzag_decode(uint32_t mapped, int width_bits)
{
    int32_t signed_val = (int32_t)(mapped >> 1) ^ -(int32_t)(mapped & 1); // XOR the one digit (1 = negative prev) with mask of ones
    uint32_t mask = (width_bits >= 32) ? 0xFFFFFFFFu : ((1u << width_bits) - 1u);
    return (uint32_t)signed_val & mask; 
}

/* Never search k all the way to width_bits when width_bits==32 -- shifting
   a 32-bit value by 32 is undefined behavior in C. Capping at 31 still
   bounds the worst case fine (see the writeup on why). */
static int max_k_for(int width_bits)
{
    return (width_bits >= 32) ? 31 : width_bits;
}

int rice_compress(const uint8_t *in, size_t in_len, sample_width_t width,
                   uint8_t *out, size_t out_cap, size_t *out_len)
{
    int width_bits = (int)width * 8;
    size_t n_samples = in_len / (size_t)width;
    size_t tail_len = in_len % (size_t)width;

    if (n_samples > RICE_MAX_SAMPLES) return -1;
    if (out_cap < sizeof(rice_header_t)) return -1;

    rice_header_t header = {
        .n_samples = (uint32_t)n_samples,
        .tail_len = (uint16_t)tail_len,
        .width = (uint8_t)width
    };
    memcpy(out, &header, sizeof(header));

    static uint32_t mapped[RICE_MAX_SAMPLES];
    uint32_t prev = 0;
    for (size_t i = 0; i < n_samples; i++) {
        uint32_t sample = read_sample(in, i, width);
        mapped[i] = zigzag_encode(sample - prev, width_bits); /* unsigned subtraction wraps on purpose */
        prev = sample;
    }

    bitwriter_t bw;
    bitwriter_init(&bw, out + sizeof(header), out_cap - sizeof(header));
    int max_k = max_k_for(width_bits);

    for (size_t block_start = 0; block_start < n_samples; block_start += RICE_BLOCK_SIZE) {
        size_t block_len = n_samples - block_start;
        if (block_len > RICE_BLOCK_SIZE) block_len = RICE_BLOCK_SIZE;
        const uint32_t *block = &mapped[block_start];

        int all_zero = 1;
        for (size_t i = 0; i < block_len; i++) {
            if (block[i] != 0) { all_zero = 0; break; }
        }
        if (all_zero) {
            if (bitwriter_put_bits(&bw, BLOCK_TYPE_ZERO, BLOCK_TYPE_BITS) != 0) return -1;
            continue;
        }

        /* Try every k, keep whichever gives the fewest total bits. */
        int best_k = 0;
        uint64_t best_cost = UINT64_MAX;
        for (int k = 0; k <= max_k; k++) {
            uint64_t cost = 0;
            for (size_t i = 0; i < block_len; i++) {
                cost += (uint64_t)(block[i] >> k) + 1 + (uint64_t)k;
            }
            if (cost < best_cost) { best_cost = cost; best_k = k; }
        }

        uint64_t verbatim_cost = (uint64_t)block_len * (uint64_t)width_bits;

        if (best_cost < verbatim_cost) {
            if (bitwriter_put_bits(&bw, BLOCK_TYPE_RICE, BLOCK_TYPE_BITS) != 0) return -1;
            if (bitwriter_put_bits(&bw, (uint32_t)best_k, K_FIELD_BITS) != 0) return -1;
            for (size_t i = 0; i < block_len; i++) {
                uint32_t q = block[i] >> best_k;
                uint32_t r = block[i] & ((1u << best_k) - 1);
                if (bitwriter_put_unary(&bw, q) != 0) return -1;
                if (best_k > 0 && bitwriter_put_bits(&bw, r, best_k) != 0) return -1;
            }
        } else {
            if (bitwriter_put_bits(&bw, BLOCK_TYPE_VERBATIM, BLOCK_TYPE_BITS) != 0) return -1;
            for (size_t i = 0; i < block_len; i++) {
                if (bitwriter_put_bits(&bw, block[i], width_bits) != 0) return -1;
            }
        }
    }

    size_t bitstream_bytes = bitwriter_flush(&bw);
    size_t total = sizeof(header) + bitstream_bytes + tail_len;
    if (total > out_cap) return -1;

    if (tail_len > 0) {
        memcpy(out + sizeof(header) + bitstream_bytes, in + n_samples * width, tail_len);
    }

    *out_len = total;
    return 0;
}

int rice_decompress(const uint8_t *in, size_t in_len, sample_width_t width,
                     uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (in_len < sizeof(rice_header_t)) return -1;

    rice_header_t header;
    memcpy(&header, in, sizeof(header));
    if (header.width != (uint8_t)width) return -1;
    if (in_len < sizeof(header) + header.tail_len) return -1;

    int width_bits = (int)width * 8;
    size_t n_samples = header.n_samples;
    size_t needed = n_samples * (size_t)width + header.tail_len;
    if (needed > out_cap || n_samples > RICE_MAX_SAMPLES) return -1;

    bitreader_t br;
    size_t bitstream_len = in_len - sizeof(header) - header.tail_len;
    bitreader_init(&br, in + sizeof(header), bitstream_len);

    uint32_t prev = 0;
    for (size_t block_start = 0; block_start < n_samples; block_start += RICE_BLOCK_SIZE) {
        size_t block_len = n_samples - block_start;
        if (block_len > RICE_BLOCK_SIZE) block_len = RICE_BLOCK_SIZE;

        uint32_t block_type = bitreader_get_bits(&br, BLOCK_TYPE_BITS);
        int k = 0;
        if (block_type == BLOCK_TYPE_RICE) {
            k = (int)bitreader_get_bits(&br, K_FIELD_BITS);
        }

        for (size_t i = 0; i < block_len; i++) {
            uint32_t mapped;
            if (block_type == BLOCK_TYPE_ZERO) {
                mapped = 0;
            } else if (block_type == BLOCK_TYPE_RICE) {
                uint32_t q = bitreader_get_unary(&br);
                uint32_t r = (k > 0) ? bitreader_get_bits(&br, k) : 0;
                mapped = (q << k) | r;
            } else {
                mapped = bitreader_get_bits(&br, width_bits);
            }

            uint32_t sample = prev + zigzag_decode(mapped, width_bits); /* unsigned add wraps, exact inverse */
            write_sample(out, block_start + i, width, sample);
            prev = sample;
        }
    }

    if (header.tail_len > 0) {
        memcpy(out + n_samples * width, in + in_len - header.tail_len, header.tail_len);
    }

    *out_len = needed;
    return 0;
}