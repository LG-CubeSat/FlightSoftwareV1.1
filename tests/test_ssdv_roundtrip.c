#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ssdv.h"
#include "ssdv_codec.h"
#include "test_jpeg_data.h"

#define ENCODED_CAPACITY (16 * SSDV_PKT_SIZE)
#define DECODED_CAPACITY (64 * 1024)

int main(void)
{
    uint8_t encoded[ENCODED_CAPACITY] = {0};
    size_t encoded_length = 0;

    if (ssdv_encode_image(test_jpeg_data, test_jpeg_data_len, "COM", 7,
                          encoded, sizeof(encoded), &encoded_length) != 0) {
        fprintf(stderr, "[FAIL] production SSDV wrapper encodes the test JPEG\n");
        return 1;
    }
    if (encoded_length == 0 || encoded_length % SSDV_RADIO_PKT_SIZE != 0) {
        fprintf(stderr, "[FAIL] encoder output is not complete SSDV packets\n");
        return 1;
    }

    ssdv_packet_info_t info = {0};
    ssdv_dec_header(&info, encoded);
    if (info.image_id != 7 || strcmp(info.callsign_s, "COM") != 0 ||
        info.width != 16 || info.height != 16) {
        fprintf(stderr, "[FAIL] SSDV packet metadata was not preserved\n");
        return 1;
    }

    uint8_t decoded_storage[DECODED_CAPACITY] = {0};
    ssdv_t decoder;
    if (ssdv_dec_init(&decoder, SSDV_RADIO_PKT_SIZE) != SSDV_OK ||
        ssdv_dec_set_buffer(&decoder, decoded_storage, sizeof(decoded_storage)) != SSDV_OK) {
        fprintf(stderr, "[FAIL] SSDV decoder initializes\n");
        return 1;
    }

    for (size_t offset = 0; offset < encoded_length; offset += SSDV_RADIO_PKT_SIZE) {
        int errors = 0;
        if (ssdv_dec_is_packet(encoded + offset, SSDV_RADIO_PKT_SIZE, &errors) != SSDV_OK) {
            fprintf(stderr, "[FAIL] encoded packet at offset %zu is invalid\n", offset);
            return 1;
        }
        if (ssdv_dec_feed(&decoder, encoded + offset) == SSDV_ERROR) {
            fprintf(stderr, "[FAIL] decoder rejected packet at offset %zu\n", offset);
            return 1;
        }
    }

    uint8_t *decoded = NULL;
    size_t decoded_length = 0;
    if (ssdv_dec_get_jpeg(&decoder, &decoded, &decoded_length) != SSDV_OK ||
        decoded_length < 4 || decoded[0] != 0xff || decoded[1] != 0xd8 ||
        decoded[decoded_length - 2] != 0xff || decoded[decoded_length - 1] != 0xd9) {
        fprintf(stderr, "[FAIL] decoded output is a complete JPEG\n");
        return 1;
    }

    printf("[PASS] encoded %u-byte JPEG into %zu bytes (%zu SSDV packets)\n",
           test_jpeg_data_len, encoded_length, encoded_length / SSDV_RADIO_PKT_SIZE);
    printf("[PASS] callsign, image id, and 16x16 dimensions survived the packet round-trip\n");
    printf("[PASS] decoder reconstructed a %zu-byte JPEG\n", decoded_length);
    printf("ssdv_roundtrip_test: PASS\n");
    return 0;
}
