#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "ccsds/profile.h"
#include "radio.h"
#include "radio_deframer.h"

/*
golden vectors: exact on-air bytes, checked against values worked out by hand
(and with python's zlib.crc32), not against our own code. plus the receive
state machine fed byte by byte with garbage, corruption and back to back packets.
*/

#define SNIFF_PORT 56250 // sim radio tx goes here, the test listens as the "ground"

static int failures = 0;

static void check(const char *name, int ok)
{
    printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) failures++;
}

/* worksheet p.20 sample telemetry: id=5, temp=2500, time=1000 */
static const uint8_t sample[] = { 0x00, 0x05, 0x09, 0xC4, 0x00, 0x00, 0x03, 0xE8 };

/* the first telemetry packet the radio sends with that sample, seq 0 */
static const uint8_t golden_packet[] = {
    0x08, 0x01, 0xC0, 0x00, 0x00, 0x1B,             // ccsds: tlm, sec hdr, apid 0x001, unsegmented seq 0, length 28 - 1
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // lg profile: version 1, raw telemetry, no compression
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, // item count 1, payload length 8
    0x00, 0x05, 0x09, 0xC4, 0x00, 0x00, 0x03, 0xE8, // payload
    0xA6, 0x30, 0x25, 0x9D                          // crc-32 of everything above
};

static int open_sniffer(void)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(SNIFF_PORT);
    if (fd < 0 || bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) return -1;
    struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    return fd;
}

static size_t build(lg_ccsds_content_type_t content, uint16_t seq, const uint8_t *payload, size_t n, uint8_t *out)
{
    size_t length = 0;
    lg_ccsds_profile_build(content, seq, 1, NULL, payload, n, out, RADIO_MAX_PACKET_SIZE, &length);
    return length;
}

/* push bytes, return how many complete packets came out */
static int feed(radio_deframer_t *d, const uint8_t *bytes, size_t n, uint8_t *last_packet, size_t *last_length)
{
    int found = 0;
    uint8_t packet[RADIO_MAX_PACKET_SIZE];
    for (size_t i = 0; i < n; i++) {
        radio_deframer_push(d, bytes[i]);
        size_t length = radio_deframer_next(d, packet);
        if (length > 0) {
            found++;
            memcpy(last_packet, packet, length);
            *last_length = length;
        }
    }
    return found;
}

int main(void)
{
    char port[16];
    snprintf(port, sizeof(port), "%d", SNIFF_PORT);
    setenv("RADIO_SIM_TX_PORT", port, 1);

    /* golden: library header matches the worksheet p.12 example exactly */
    ccsds_space_packet_header_t header = {
        .type = CCSDS_PACKET_TYPE_TELEMETRY,
        .has_secondary_header = false,
        .apid = 0x042,
        .sequence_flags = CCSDS_SEQUENCE_UNSEGMENTED,
        .sequence_count = 5
    };
    uint8_t eight[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    uint8_t space_packet[32];
    size_t space_length = 0;
    ccsds_space_packet_build(&header, eight, sizeof(eight), space_packet, sizeof(space_packet), &space_length);
    const uint8_t worksheet_header[] = { 0x00, 0x42, 0xC0, 0x05, 0x00, 0x07 };
    check("worksheet example header is 00 42 C0 05 00 07, 14 bytes total",
          space_length == 14 && memcmp(space_packet, worksheet_header, 6) == 0);

    /* golden: what the radio actually puts on the air */
    int sniffer = open_sniffer();
    check("sniffer socket open", sniffer >= 0);

    check("queue sample telemetry", radio_send(sample, sizeof(sample)) == 0);
    uint8_t air[RADIO_MAX_PACKET_SIZE + 1];
    ssize_t n = recv(sniffer, air, sizeof(air), 0);
    check("on-air telemetry packet matches the golden bytes exactly",
          n == (ssize_t)sizeof(golden_packet) && memcmp(air, golden_packet, sizeof(golden_packet)) == 0);

    uint8_t ssdv[2 * LG_CCSDS_SSDV_PACKET_SIZE];
    memset(ssdv, 0x55, sizeof(ssdv));
    check("queue two ssdv packets", radio_send_ssdv(ssdv, sizeof(ssdv)) == 0);
    n = recv(sniffer, air, sizeof(air), 0);
    const uint8_t ssdv_header[] = { 0x08, 0x03, 0xC0, 0x00, 0x00, 0xE9 };
    check("ssdv packet is exactly 240 bytes, apid 0x003, seq 0, length 233",
          n == RADIO_MAX_PACKET_SIZE && memcmp(air, ssdv_header, sizeof(ssdv_header)) == 0);
    n = recv(sniffer, air, sizeof(air), 0);
    check("second ssdv packet has seq 1", n == RADIO_MAX_PACKET_SIZE && air[2] == 0xC0 && air[3] == 0x01);

    check("ssdv length not a multiple of 214 rejected", radio_send_ssdv(ssdv, 100) == -1);

    /* receive state machine */
    radio_deframer_t d;
    uint8_t out[RADIO_MAX_PACKET_SIZE];
    size_t out_length = 0;

    radio_deframer_reset(&d);
    check("golden packet fed byte by byte comes out whole",
          feed(&d, golden_packet, sizeof(golden_packet), out, &out_length) == 1 &&
          out_length == sizeof(golden_packet) && memcmp(out, golden_packet, out_length) == 0);

    radio_deframer_reset(&d);
    const uint8_t garbage[] = { 0xFF, 0x08, 0x00, 0x13, 0x37, 0x08, 0x01, 0xC0, 0x99 };
    int found = feed(&d, garbage, sizeof(garbage), out, &out_length);
    found += feed(&d, golden_packet, sizeof(golden_packet), out, &out_length);
    check("garbage before a packet is skipped, packet still found",
          found == 1 && memcmp(out, golden_packet, sizeof(golden_packet)) == 0);

    uint8_t corrupt[sizeof(golden_packet)];
    memcpy(corrupt, golden_packet, sizeof(corrupt));
    corrupt[25] ^= 0x01; // one flipped bit in the payload
    radio_deframer_reset(&d);
    found = feed(&d, corrupt, sizeof(corrupt), out, &out_length);
    check("packet with a flipped bit is rejected", found == 0);
    found = feed(&d, golden_packet, sizeof(golden_packet), out, &out_length);
    check("good packet right after a corrupt one is still found",
          found == 1 && memcmp(out, golden_packet, sizeof(golden_packet)) == 0);

    uint8_t two[2 * RADIO_MAX_PACKET_SIZE];
    uint8_t a_payload[50];
    uint8_t b_payload[90];
    memset(a_payload, 0xAA, sizeof(a_payload));
    memset(b_payload, 0xBB, sizeof(b_payload));
    size_t a_length = build(LG_CCSDS_CONTENT_RAW_TELEMETRY, 7, a_payload, sizeof(a_payload), two);
    size_t b_length = build(LG_CCSDS_CONTENT_COMMAND, 8, b_payload, sizeof(b_payload), two + a_length);
    radio_deframer_reset(&d);
    found = feed(&d, two, a_length + b_length, out, &out_length);
    check("two packets back to back both come out", found == 2 && out_length == b_length);

    radio_deframer_reset(&d);
    found = feed(&d, golden_packet, 20, out, &out_length);
    check("half a packet waits for the rest", found == 0 && radio_deframer_buffered(&d) == 20);
    found = feed(&d, golden_packet + 20, sizeof(golden_packet) - 20, out, &out_length);
    check("rest of the packet completes it", found == 1 && out_length == sizeof(golden_packet));

    close(sniffer);
    if (failures != 0) {
        printf("radio_framing_test: %d FAILED\n", failures);
        return 1;
    }
    printf("radio_framing_test: PASS\n");
    return 0;
}
