#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "frame.h"

static int failures = 0;

#define CHECK(condition, message) do {                                      \
    if (condition) {                                                        \
        printf("[PASS] %s\n", message);                                    \
    } else {                                                               \
        fprintf(stderr, "[FAIL] %s\n", message);                           \
        failures++;                                                        \
    }                                                                      \
} while (0)

int main(void)
{
    const uint8_t payload[] = {0x10, 0x20, 0x30};
    Frame original = {
        .dest_addr = 0x02,
        .src_addr = 0x01,
        .length = sizeof(payload),
    };
    memcpy(original.payload, payload, sizeof(payload));

    uint8_t wire[4 + MAX_FRAME_PAYLOAD] = {0};
    int wire_len = frame_serialize(&original, wire, sizeof(wire));
    CHECK(wire_len == 7, "serialize returns header plus payload length");
    CHECK(wire[0] == 0x02 && wire[1] == 0x01 &&
          wire[2] == 0x00 && wire[3] == 0x03,
          "serialize writes addresses and network-order length");

    Frame decoded = {0};
    int decoded_len = frame_deserialize(wire, wire_len, &decoded);
    CHECK(decoded_len == wire_len, "deserialize consumes the complete frame");
    CHECK(decoded.dest_addr == original.dest_addr &&
          decoded.src_addr == original.src_addr &&
          decoded.length == original.length &&
          memcmp(decoded.payload, original.payload, original.length) == 0,
          "serialize/deserialize round-trip preserves the frame");

    CHECK(frame_serialize(&original, wire, 6) == -1,
          "serialize rejects an undersized output buffer");
    CHECK(frame_deserialize(wire, 3, &decoded) == -1,
          "deserialize rejects a truncated header");

    const uint8_t truncated[] = {0x02, 0x01, 0x00, 0x04, 0xaa, 0xbb};
    CHECK(frame_deserialize(truncated, sizeof(truncated), &decoded) == -1,
          "deserialize rejects a payload shorter than its declared length");

    if (failures != 0) {
        fprintf(stderr, "frame_codec_test: FAIL (%d checks failed)\n", failures);
        return 1;
    }

    printf("frame_codec_test: PASS\n");
    return 0;
}
