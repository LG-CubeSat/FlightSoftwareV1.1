/*
 * Direct-linked correctness test for the Rice codec (apps/obc/compute) --
 * no processes, no IPC, just calling rice_compress/rice_decompress and
 * checking the round trip, same style as comms_bus_test. This is the
 * cheapest place to catch a bit-packing or delta/zig-zag bug, before it's
 * buried under compute's process/IPC plumbing.
 *
 * Two real bugs were caught by exactly this kind of round-trip testing
 * during development: an unmasked delta computation that only wrapped at
 * 32 bits regardless of the actual sample width (silently corrupted width
 * 1/2 data), and undefined behavior in a signed left-shift when reading a
 * width-4 sample with a high byte value. Both are exercised below.
 *
 * Run directly:
 *   ./build/tests/rice_codec_test
 * Or via CTest:
 *   ctest --test-dir build -R rice_codec_test --output-on-failure
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "rice_codec.h"

static int total_checks = 0;
static int failed_checks = 0;

static void round_trip(const uint8_t *data, size_t len, sample_width_t width, const char *label) {
    static uint8_t compressed[1 << 20];
    static uint8_t decompressed[1 << 20];
    size_t clen = 0, dlen = 0;

    total_checks++;
    if (rice_compress(data, len, width, compressed, sizeof(compressed), &clen) != 0) {
        printf("[CHECK] FAIL: %s (compress failed)\n", label);
        failed_checks++;
        return;
    }

    if (rice_decompress(compressed, clen, width, decompressed, sizeof(decompressed), &dlen) != 0) {
        printf("[CHECK] FAIL: %s (decompress failed)\n", label);
        failed_checks++;
        return;
    }

    if (dlen != len || memcmp(data, decompressed, len) != 0) {
        printf("[CHECK] FAIL: %s (round trip mismatch: in=%zu compressed=%zu out=%zu)\n", label, len, clen, dlen);
        failed_checks++;
        return;
    }

    printf("[CHECK] PASS: %s (in=%zu compressed=%zu)\n", label, len, clen);
}

int main(void) {
    /* all-zero -- exercises the zero-block path */
    static uint8_t zeros[100] = {0};
    round_trip(zeros, sizeof(zeros), SAMPLE_WIDTH_8, "all-zero, width 1");

    /* constant non-zero */
    static uint8_t constant[100];
    memset(constant, 42, sizeof(constant));
    round_trip(constant, sizeof(constant), SAMPLE_WIDTH_8, "constant value, width 1");

    /* smooth ramp -- delta should compress this well */
    static uint8_t ramp[200];
    for (size_t i = 0; i < sizeof(ramp); i++) ramp[i] = (uint8_t)(i % 256);
    round_trip(ramp, sizeof(ramp), SAMPLE_WIDTH_8, "smooth ramp, width 1");

    /* random/incompressible -- exercises the verbatim fallback. The real
       bug this specifically caught: an unmasked 32-bit delta computation
       corrupted any residual that wrapped negative at 8-bit width -- only
       shows up with data that actually produces such a wrap, which a
       smooth ramp never does. */
    static uint8_t random_data[150];
    srand(42);
    for (size_t i = 0; i < sizeof(random_data); i++) random_data[i] = (uint8_t)rand();
    round_trip(random_data, sizeof(random_data), SAMPLE_WIDTH_8, "random bytes, width 1");

    /* odd length, not a multiple of the block size or the sample width */
    static uint8_t odd[37];
    for (size_t i = 0; i < sizeof(odd); i++) odd[i] = (uint8_t)(i * 7);
    round_trip(odd, sizeof(odd), SAMPLE_WIDTH_8, "odd length 37, width 1");

    /* width 4 with high byte values (>= 128) in every byte position --
       the real bug this specifically caught: reading the top byte of a
       32-bit sample via a signed left shift is undefined behavior in C
       for byte values >= 128 (confirmed by UBSan during development). */
    static uint8_t high_bytes[64];
    for (size_t i = 0; i < sizeof(high_bytes); i++) high_bytes[i] = (uint8_t)(200 + i);
    round_trip(high_bytes, sizeof(high_bytes), SAMPLE_WIDTH_32, "high byte values (>=128), width 4");

    /* width 2, ramp */
    static uint8_t ramp16[80];
    for (size_t i = 0; i < sizeof(ramp16); i++) ramp16[i] = (uint8_t)(i * 3);
    round_trip(ramp16, sizeof(ramp16), SAMPLE_WIDTH_16, "ramp, width 2");

    /* not a multiple of width -- exercises the tail-byte path */
    static uint8_t tail_case[19];
    for (size_t i = 0; i < sizeof(tail_case); i++) tail_case[i] = (uint8_t)(i * 13);
    round_trip(tail_case, sizeof(tail_case), SAMPLE_WIDTH_32, "19 bytes (tail bytes), width 4");

    /* empty input -- a real (non-NULL) zero-length buffer, since passing
       NULL to memcmp is technically UB even with length 0 */
    static uint8_t empty[1];
    round_trip(empty, 0, SAMPLE_WIDTH_8, "empty input, width 1");

    if (failed_checks == 0) {
        printf("rice_codec_test: PASS (%d/%d checks)\n", total_checks, total_checks);
        return 0;
    }

    fprintf(stderr, "rice_codec_test: FAIL (%d/%d checks failed)\n", failed_checks, total_checks);
    return 1;
}
