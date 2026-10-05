#include <stdio.h>
#include <string.h>

#include "ccsds/ccsds.h"

static int checks_run = 0;
static int checks_failed = 0;

static void check_true(const char *label, int condition)
{
    ++checks_run;
    if (condition) {
        printf("[CHECK] PASS: %s\n", label);
    } else {
        printf("[CHECK] FAIL: %s\n", label);
        ++checks_failed;
    }
}

static void test_crc32(void)
{
    static const uint8_t check_text[] = "123456789";

    check_true(
        "CRC-32/ISO-HDLC check value",
        ccsds_crc32(check_text, sizeof(check_text) - 1U) == UINT32_C(0xCBF43926));
}

static void test_space_packet(void)
{
    static const uint8_t data[] = {0xAAU, 0xBBU, 0xCCU};
    static const uint8_t expected_header[] = {
        0x08U, 0x02U, 0xC1U, 0x23U, 0x00U, 0x02U
    };
    ccsds_space_packet_header_t header = {
        .type = CCSDS_PACKET_TYPE_TELEMETRY,
        .has_secondary_header = true,
        .apid = LG_CCSDS_APID_COMPRESSED_TELEMETRY,
        .sequence_flags = CCSDS_SEQUENCE_UNSEGMENTED,
        .sequence_count = 0x0123U
    };
    uint8_t packet[32];
    size_t packet_length = 0U;
    ccsds_space_packet_view_t view;
    ccsds_status_t status;

    status = ccsds_space_packet_build(
        &header, data, sizeof(data), packet, sizeof(packet), &packet_length);
    check_true("Space Packet builds", status == CCSDS_OK);
    check_true(
        "Space Packet primary header matches the wire format",
        packet_length == sizeof(expected_header) + sizeof(data) &&
        memcmp(packet, expected_header, sizeof(expected_header)) == 0);

    status = ccsds_space_packet_parse(packet, packet_length, &view);
    check_true(
        "Space Packet parses back to the same fields",
        status == CCSDS_OK &&
        view.header.apid == header.apid &&
        view.header.sequence_count == header.sequence_count &&
        view.data_length == sizeof(data) &&
        memcmp(view.data, data, sizeof(data)) == 0);

    status = ccsds_space_packet_parse(packet, packet_length - 1U, &view);
    check_true("Truncated Space Packet is rejected", status == CCSDS_ERROR_BUFFER_TOO_SMALL);

    packet[0] |= 0x20U;
    status = ccsds_space_packet_parse(packet, packet_length, &view);
    check_true("Unsupported packet version is rejected", status == CCSDS_ERROR_UNSUPPORTED);
}

static const ccsds_121_config_t test_config = {
    .bits_per_sample = 12U,
    .block_size = 8U,
    .reference_sample_interval = 4U,
    .signed_samples = true,
    .preprocess = true,
    .restricted_code_options = false
};

/* 12-bit, two's-complement samples in the low 12 bits, network byte order. */
static const uint8_t test_samples[] = {
    0x0FU, 0xF6U, 0x0FU, 0xF7U, 0x0FU, 0xF7U, 0x00U, 0x00U,
    0x00U, 0x01U, 0x00U, 0xC8U, 0x07U, 0xFFU, 0x08U, 0x00U,
    0x08U, 0x00U, 0x08U, 0x01U, 0x00U, 0xC9U, 0x00U, 0x02U,
    0x00U, 0x01U, 0x00U, 0x00U, 0x0FU, 0xFFU, 0x0FU, 0xFEU
};

static size_t storage_for_bits(uint8_t bits)
{
    return (size_t)(bits + 7U) / 8U;
}

static void write_test_sample(
    uint8_t *output,
    size_t storage_size,
    uint8_t bits,
    uint32_t value)
{
    const uint32_t mask = bits == 32U ? UINT32_MAX :
                          (UINT32_C(1) << bits) - 1U;
    size_t byte_index;

    value &= mask;
    for (byte_index = 0U; byte_index < storage_size; ++byte_index) {
        output[storage_size - 1U - byte_index] = (uint8_t)(value >> (8U * byte_index));
    }
}

static void test_compression_matrix(void)
{
    static const uint8_t widths[] = {
        1U, 2U, 3U, 4U, 7U, 8U, 9U, 12U, 16U, 17U, 24U, 25U, 32U
    };
    static const uint8_t block_sizes[] = {8U, 16U, 32U, 64U};
    uint8_t samples[64U * 4U];
    uint8_t decoded[sizeof(samples)];
    uint8_t compressed[1024];
    int matrix_ok = 1;
    size_t width_index;
    size_t block_index;

    for (width_index = 0U; width_index < sizeof(widths); ++width_index) {
        for (block_index = 0U; block_index < sizeof(block_sizes); ++block_index) {
            const size_t storage_size = storage_for_bits(widths[width_index]);
            const size_t sample_count = block_sizes[block_index];
            const size_t samples_length = storage_size * sample_count;
            ccsds_121_config_t config = {
                .bits_per_sample = widths[width_index],
                .block_size = block_sizes[block_index],
                .reference_sample_interval = 2U,
                .signed_samples = false,
                .preprocess = true,
                .restricted_code_options = widths[width_index] <= 4U
            };
            size_t compressed_length = 0U;
            size_t decoded_length = 0U;
            size_t sample_index;
            ccsds_status_t status;

            memset(samples, 0, sizeof(samples));
            memset(decoded, 0, sizeof(decoded));
            for (sample_index = 0U; sample_index < sample_count; ++sample_index) {
                write_test_sample(
                    samples + sample_index * storage_size,
                    storage_size,
                    widths[width_index],
                    (uint32_t)(sample_index * 3U + width_index));
            }

            status = ccsds_121_encode(
                &config, samples, sample_count,
                compressed, sizeof(compressed), &compressed_length);
            if (status == CCSDS_OK) {
                status = ccsds_121_decode(
                    &config, compressed, compressed_length, sample_count,
                    decoded, sizeof(decoded), &decoded_length);
            }
            if (status != CCSDS_OK || decoded_length != samples_length ||
                memcmp(samples, decoded, samples_length) != 0) {
                fprintf(stderr, "matrix failure: bits=%u block=%u status=%s\n",
                        widths[width_index], block_sizes[block_index],
                        ccsds_status_string(status));
                matrix_ok = 0;
            }
        }
    }
    check_true(
        "CCSDS 121 round trips all standard widths and block sizes",
        matrix_ok);
}

static void test_compression_and_profile(void)
{
    uint8_t compressed[512];
    uint8_t decoded[sizeof(test_samples)];
    uint8_t packet[1024];
    uint8_t corrupted[1024];
    size_t compressed_length = 0U;
    size_t decoded_length = 0U;
    size_t packet_length = 0U;
    size_t bound = 0U;
    lg_ccsds_profile_view_t view;
    ccsds_status_t status;

    status = ccsds_121_encoded_bound(&test_config, 16U, &bound);
    check_true(
        "CCSDS 121 reports a conservative output bound",
        status == CCSDS_OK && bound >= sizeof(test_samples));

    {
        ccsds_121_config_t invalid_config = test_config;
        invalid_config.restricted_code_options = true;
        check_true(
            "Restricted code options reject samples wider than four bits",
            ccsds_121_validate_config(&invalid_config) == CCSDS_ERROR_RANGE);
    }

    status = ccsds_121_encode(
        &test_config,
        test_samples,
        16U,
        compressed,
        sizeof(compressed),
        &compressed_length);
    check_true(
        "CCSDS 121 encodes 12-bit signed samples",
        status == CCSDS_OK && compressed_length > 0U);

    status = ccsds_121_decode(
        &test_config,
        compressed,
        compressed_length,
        16U,
        decoded,
        sizeof(decoded),
        &decoded_length);
    check_true(
        "CCSDS 121 round trip is byte exact",
        status == CCSDS_OK && decoded_length == sizeof(test_samples) &&
        memcmp(decoded, test_samples, sizeof(test_samples)) == 0);

    status = lg_ccsds_profile_build(
        LG_CCSDS_CONTENT_COMPRESSED_TELEMETRY,
        42U,
        16U,
        &test_config,
        compressed,
        compressed_length,
        packet,
        sizeof(packet),
        &packet_length);
    check_true("Compressed telemetry profile packet builds", status == CCSDS_OK);

    status = lg_ccsds_profile_parse(packet, packet_length, &view);
    check_true(
        "Compressed telemetry profile packet parses and verifies",
        status == CCSDS_OK &&
        view.metadata.content_type == LG_CCSDS_CONTENT_COMPRESSED_TELEMETRY &&
        view.metadata.sequence_count == 42U &&
        view.metadata.item_count == 16U &&
        view.metadata.compression.bits_per_sample == 12U &&
        view.payload_length == compressed_length &&
        memcmp(view.payload, compressed, compressed_length) == 0);

    memcpy(corrupted, packet, packet_length);
    corrupted[CCSDS_SPACE_PACKET_PRIMARY_HEADER_SIZE +
              LG_CCSDS_PROFILE_HEADER_SIZE] ^= 0x01U;
    status = lg_ccsds_profile_parse(corrupted, packet_length, &view);
    check_true("Changed payload byte fails CRC", status == CCSDS_ERROR_CRC);

    {
        uint8_t invalid_samples[sizeof(test_samples)];
        memcpy(invalid_samples, test_samples, sizeof(test_samples));
        invalid_samples[0] |= 0x80U;
        status = ccsds_121_encode(
            &test_config,
            invalid_samples,
            16U,
            compressed,
            sizeof(compressed),
            &compressed_length);
        check_true("Nonzero unused sample bits are rejected", status == CCSDS_ERROR_RANGE);
    }
}

static void test_ssdv_passthrough(void)
{
    uint8_t ssdv[LG_CCSDS_SSDV_PACKET_SIZE];
    uint8_t packet[512];
    size_t packet_length = 0U;
    lg_ccsds_profile_view_t view;
    ccsds_status_t status;
    size_t index;

    for (index = 0U; index < sizeof(ssdv); ++index) {
        ssdv[index] = (uint8_t)(index ^ 0x5AU);
    }

    status = lg_ccsds_profile_build(
        LG_CCSDS_CONTENT_SSDV,
        7U,
        1U,
        NULL,
        ssdv,
        sizeof(ssdv),
        packet,
        sizeof(packet),
        &packet_length);
    check_true("One SSDV packet fits in one profile packet", status == CCSDS_OK);

    status = lg_ccsds_profile_parse(packet, packet_length, &view);
    check_true(
        "SSDV survives packet wrapping byte for byte",
        status == CCSDS_OK &&
        view.metadata.content_type == LG_CCSDS_CONTENT_SSDV &&
        view.payload_length == sizeof(ssdv) &&
        memcmp(view.payload, ssdv, sizeof(ssdv)) == 0);
}

typedef struct {
    size_t callback_count;
    lg_ccsds_content_type_t content_types[2];
} stream_test_context_t;

static ccsds_status_t stream_callback(
    const uint8_t *packet,
    size_t packet_length,
    void *user_context)
{
    stream_test_context_t *context = (stream_test_context_t *)user_context;
    lg_ccsds_profile_view_t view;
    ccsds_status_t status = lg_ccsds_profile_parse(packet, packet_length, &view);

    if (status != CCSDS_OK || context->callback_count >= 2U) {
        return CCSDS_ERROR_INVALID_FORMAT;
    }
    context->content_types[context->callback_count] = view.metadata.content_type;
    ++context->callback_count;
    return CCSDS_OK;
}

static void test_stream_parser(void)
{
    static const uint8_t telemetry[] = {1U, 2U, 3U, 4U};
    static const uint8_t command[] = {0xA5U, 0x01U};
    uint8_t packet_a[128];
    uint8_t packet_b[128];
    uint8_t both[256];
    uint8_t parser_storage[128];
    size_t packet_a_length = 0U;
    size_t packet_b_length = 0U;
    size_t delivered = 0U;
    size_t index;
    ccsds_stream_parser_t parser;
    stream_test_context_t context = {0};
    ccsds_status_t status;

    status = lg_ccsds_profile_build(
        LG_CCSDS_CONTENT_RAW_TELEMETRY, 1U, 1U, NULL,
        telemetry, sizeof(telemetry), packet_a, sizeof(packet_a), &packet_a_length);
    check_true("Raw telemetry packet builds", status == CCSDS_OK);
    status = lg_ccsds_profile_build(
        LG_CCSDS_CONTENT_COMMAND, 2U, 1U, NULL,
        command, sizeof(command), packet_b, sizeof(packet_b), &packet_b_length);
    check_true("Command packet builds", status == CCSDS_OK);

    memcpy(both, packet_a, packet_a_length);
    memcpy(both + packet_a_length, packet_b, packet_b_length);
    status = ccsds_stream_parser_init(&parser, parser_storage, sizeof(parser_storage));
    check_true("Stream parser initializes", status == CCSDS_OK);

    /* Feed the first packet one byte at a time to simulate UART chunks. */
    for (index = 0U; index < packet_a_length; ++index) {
        size_t just_delivered = 0U;
        status = ccsds_stream_parser_feed(
            &parser, both + index, 1U, stream_callback, &context, &just_delivered);
        if (status != CCSDS_OK) {
            break;
        }
        delivered += just_delivered;
    }
    check_true(
        "Stream parser reassembles a packet delivered byte by byte",
        status == CCSDS_OK && delivered == 1U);

    delivered = 0U;
    status = ccsds_stream_parser_feed(
        &parser,
        both + packet_a_length,
        packet_b_length,
        stream_callback,
        &context,
        &delivered);
    check_true(
        "Stream parser accepts a complete following packet",
        status == CCSDS_OK && delivered == 1U && context.callback_count == 2U &&
        context.content_types[0] == LG_CCSDS_CONTENT_RAW_TELEMETRY &&
        context.content_types[1] == LG_CCSDS_CONTENT_COMMAND);

    memset(&context, 0, sizeof(context));
    ccsds_stream_parser_reset(&parser);
    delivered = 0U;
    status = ccsds_stream_parser_feed(
        &parser,
        both,
        packet_a_length + packet_b_length,
        stream_callback,
        &context,
        &delivered);
    check_true(
        "Stream parser emits two packets from one radio chunk",
        status == CCSDS_OK && delivered == 2U && context.callback_count == 2U);

    {
        uint8_t small_storage[8];
        ccsds_stream_parser_t small_parser;

        status = ccsds_stream_parser_init(
            &small_parser, small_storage, sizeof(small_storage));
        delivered = 0U;
        if (status == CCSDS_OK) {
            status = ccsds_stream_parser_feed(
                &small_parser,
                packet_a,
                CCSDS_SPACE_PACKET_PRIMARY_HEADER_SIZE,
                stream_callback,
                &context,
                &delivered);
        }
        check_true(
            "Stream parser rejects a packet larger than its storage",
            status == CCSDS_ERROR_BUFFER_TOO_SMALL && delivered == 0U);
    }
}

int main(void)
{
    test_crc32();
    test_space_packet();
    test_compression_matrix();
    test_compression_and_profile();
    test_ssdv_passthrough();
    test_stream_parser();

    if (checks_failed == 0) {
        printf("ccsds_test: PASS (%d checks)\n", checks_run);
        return 0;
    }
    fprintf(stderr, "ccsds_test: FAIL (%d of %d checks failed)\n",
            checks_failed, checks_run);
    return 1;
}
