#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ccsds/ccsds.h"

static int run_demo(void)
{
    static const uint8_t samples[] = {
        0x03U, 0xE8U, 0x03U, 0xE9U, 0x03U, 0xEAU, 0x03U, 0xEBU,
        0x03U, 0xECU, 0x03U, 0xEDU, 0x03U, 0xEEU, 0x03U, 0xEFU,
        0x03U, 0xF0U, 0x03U, 0xF1U, 0x03U, 0xF2U, 0x03U, 0xF3U,
        0x03U, 0xF4U, 0x03U, 0xF5U, 0x03U, 0xF6U, 0x03U, 0xF7U
    };
    const ccsds_121_config_t config = {
        .bits_per_sample = 16U,
        .block_size = 8U,
        .reference_sample_interval = 4U,
        .signed_samples = false,
        .preprocess = true,
        .restricted_code_options = false
    };
    uint8_t compressed[512];
    uint8_t packet[1024];
    uint8_t recovered[sizeof(samples)];
    size_t compressed_length = 0U;
    size_t packet_length = 0U;
    size_t recovered_length = 0U;
    lg_ccsds_profile_view_t view;
    ccsds_status_t status;

    status = ccsds_121_encode(
        &config, samples, 16U, compressed, sizeof(compressed), &compressed_length);
    if (status != CCSDS_OK) {
        fprintf(stderr, "compression failed: %s\n", ccsds_status_string(status));
        return 1;
    }
    status = lg_ccsds_profile_build(
        LG_CCSDS_CONTENT_COMPRESSED_TELEMETRY,
        0U,
        16U,
        &config,
        compressed,
        compressed_length,
        packet,
        sizeof(packet),
        &packet_length);
    if (status != CCSDS_OK) {
        fprintf(stderr, "packet build failed: %s\n", ccsds_status_string(status));
        return 1;
    }
    status = lg_ccsds_profile_parse(packet, packet_length, &view);
    if (status != CCSDS_OK) {
        fprintf(stderr, "packet parse failed: %s\n", ccsds_status_string(status));
        return 1;
    }
    status = ccsds_121_decode(
        &view.metadata.compression,
        view.payload,
        view.payload_length,
        view.metadata.item_count,
        recovered,
        sizeof(recovered),
        &recovered_length);
    if (status != CCSDS_OK || recovered_length != sizeof(samples) ||
        memcmp(samples, recovered, sizeof(samples)) != 0) {
        fprintf(stderr, "round trip failed: %s\n", ccsds_status_string(status));
        return 1;
    }

    printf("LG-CubeSat CCSDS Profile v%u demo\n", LG_CCSDS_PROFILE_VERSION);
    printf("  Input:      %zu bytes (16 unsigned 16-bit samples)\n", sizeof(samples));
    printf("  Compressed: %zu bytes (CCSDS 121.0-B-3)\n", compressed_length);
    printf("  Packet:     %zu bytes (CCSDS Space Packet + profile CRC)\n", packet_length);
    printf("  APID:       0x%03X\n", view.space_packet.header.apid);
    printf("  Result:     recovered samples match exactly\n");
    return 0;
}

static int inspect_file(const char *path)
{
    FILE *file;
    long file_size;
    uint8_t *packet;
    size_t bytes_read;
    lg_ccsds_profile_view_t view;
    ccsds_status_t status;

    file = fopen(path, "rb");
    if (file == NULL) {
        perror(path);
        return 1;
    }
    if (fseek(file, 0L, SEEK_END) != 0 ||
        (file_size = ftell(file)) < 0L ||
        fseek(file, 0L, SEEK_SET) != 0 ||
        (unsigned long)file_size > CCSDS_SPACE_PACKET_MAX_TOTAL_SIZE) {
        fprintf(stderr, "%s: invalid or oversized packet file\n", path);
        fclose(file);
        return 1;
    }

    packet = (uint8_t *)malloc((size_t)file_size);
    if (packet == NULL) {
        fprintf(stderr, "not enough memory for %ld-byte packet\n", file_size);
        fclose(file);
        return 1;
    }
    bytes_read = fread(packet, 1U, (size_t)file_size, file);
    fclose(file);
    if (bytes_read != (size_t)file_size) {
        fprintf(stderr, "%s: short read\n", path);
        free(packet);
        return 1;
    }

    status = lg_ccsds_profile_parse(packet, bytes_read, &view);
    if (status != CCSDS_OK) {
        fprintf(stderr, "%s: %s\n", path, ccsds_status_string(status));
        free(packet);
        return 1;
    }

    printf("Profile version: %u\n", LG_CCSDS_PROFILE_VERSION);
    printf("Direction:       %s\n",
           view.space_packet.header.type == CCSDS_PACKET_TYPE_COMMAND ?
               "uplink command" : "downlink telemetry");
    printf("APID:            0x%03X\n", view.space_packet.header.apid);
    printf("Sequence count:  %u\n", view.metadata.sequence_count);
    printf("Content:         %s\n",
           lg_ccsds_content_type_name(view.metadata.content_type));
    printf("Item count:      %u\n", view.metadata.item_count);
    printf("Payload bytes:   %zu\n", view.payload_length);
    printf("CRC-32:          0x%08X (valid)\n", view.crc32);
    if (view.metadata.content_type == LG_CCSDS_CONTENT_COMPRESSED_TELEMETRY) {
        printf("Sample bits:     %u\n", view.metadata.compression.bits_per_sample);
        printf("Block size:      %u\n", view.metadata.compression.block_size);
        printf("Reference span:  %u blocks\n",
               view.metadata.compression.reference_sample_interval);
    }

    free(packet);
    return 0;
}

static void print_usage(const char *program)
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s demo\n", program);
    fprintf(stderr, "  %s inspect PACKET.bin\n", program);
}

int main(int argc, char **argv)
{
    if (argc == 1 || (argc == 2 && strcmp(argv[1], "demo") == 0)) {
        return run_demo();
    }
    if (argc == 3 && strcmp(argv[1], "inspect") == 0) {
        return inspect_file(argv[2]);
    }
    print_usage(argv[0]);
    return 2;
}
