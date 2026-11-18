#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "ccsds/profile.h"
#include "radio.h"
#include "radio_queue.h"

#define TEST_PORT 56240 // tx + rx on the same port = loopback

static int failures = 0;

static void check(const char *name, int ok)
{
    printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) failures++;
}

static void fill(uint8_t *buf, size_t length, uint8_t seed)
{
    for (size_t i = 0; i < length; i++) buf[i] = (uint8_t)(seed + i);
}

/* receive payloads until `length` bytes arrive or nothing more comes */
static size_t receive_all(uint8_t *out, size_t length, int *packets)
{
    uint8_t got[RADIO_MAX_PAYLOAD];
    size_t total = 0;
    *packets = 0;
    while (total < length) {
        int n = radio_receive(got, sizeof(got));
        if (n <= 0 || total + (size_t)n > length) break;
        memcpy(out + total, got, (size_t)n);
        total += (size_t)n;
        (*packets)++;
    }
    return total;
}

static void send_raw(const uint8_t *bytes, size_t length)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(TEST_PORT);
    sendto(fd, bytes, length, 0, (struct sockaddr *)&addr, sizeof(addr));
    close(fd);
}

int main(void)
{
    char port[16];
    snprintf(port, sizeof(port), "%d", TEST_PORT);
    setenv("RADIO_SIM_TX_PORT", port, 1);
    setenv("RADIO_SIM_RX_PORT", port, 1);
    setenv("RADIO_SIM_RX_TIMEOUT_MS", "200", 1);

    uint8_t sent[1000];
    uint8_t got[1000];
    int packets = 0;

    check("receive with nothing sent times out", radio_receive(got, RADIO_MAX_PAYLOAD) == 0);

    /* telemetry is cut into <= 214 byte payloads, each its own ccsds packet */
    fill(sent, 1, 3);
    check("send 1 byte", radio_send(sent, 1) == 0);
    check("1 byte arrives as one packet", receive_all(got, 1, &packets) == 1 && packets == 1 && got[0] == sent[0]);

    fill(sent, RADIO_MAX_PAYLOAD, 1);
    check("send one full 214 byte payload", radio_send(sent, RADIO_MAX_PAYLOAD) == 0);
    check("214 bytes arrive as one packet",
          receive_all(got, RADIO_MAX_PAYLOAD, &packets) == RADIO_MAX_PAYLOAD && packets == 1 &&
          memcmp(got, sent, RADIO_MAX_PAYLOAD) == 0);

    fill(sent, 500, 7);
    check("send 500 bytes", radio_send(sent, 500) == 0);
    check("500 bytes arrive in order as 214 + 214 + 72",
          receive_all(got, 500, &packets) == 500 && packets == 3 && memcmp(got, sent, 500) == 0);

    /* ssdv: one ssdv packet per radio packet */
    fill(sent, 3 * LG_CCSDS_SSDV_PACKET_SIZE, 9);
    check("send 3 ssdv packets", radio_send_ssdv(sent, 3 * LG_CCSDS_SSDV_PACKET_SIZE) == 0);
    check("ssdv arrives as exactly 3 packets of 214",
          receive_all(got, 3 * LG_CCSDS_SSDV_PACKET_SIZE, &packets) == 3 * LG_CCSDS_SSDV_PACKET_SIZE &&
          packets == 3 && memcmp(got, sent, 3 * LG_CCSDS_SSDV_PACKET_SIZE) == 0);
    check("ssdv that isn't whole 214 byte packets rejected", radio_send_ssdv(sent, 300) == -1);

    /* arq: the ground says ssdv packet seq 1 never arrived */
    check("resend ssdv seq 1", radio_resend(LG_CCSDS_APID_SSDV, 1) == 0);
    check("resent packet is the second ssdv packet again",
          receive_all(got, LG_CCSDS_SSDV_PACKET_SIZE, &packets) == LG_CCSDS_SSDV_PACKET_SIZE &&
          memcmp(got, sent + LG_CCSDS_SSDV_PACKET_SIZE, LG_CCSDS_SSDV_PACKET_SIZE) == 0);
    check("resend of a packet never sent fails", radio_resend(LG_CCSDS_APID_SSDV, 500) == -1);
    check("resend with the wrong apid fails", radio_resend(LG_CCSDS_APID_COMMAND, 1) == -1);

    /* back to back sends stay in order */
    uint8_t first[10];
    uint8_t second[20];
    fill(first, sizeof(first), 20);
    fill(second, sizeof(second), 40);
    radio_send(first, sizeof(first));
    radio_send(second, sizeof(second));
    check("back to back sends arrive in order",
          receive_all(got, 30, &packets) == 30 && memcmp(got, first, 10) == 0 && memcmp(got + 10, second, 20) == 0);
    check("nothing left over after back to back", radio_receive(got, RADIO_MAX_PAYLOAD) == 0);

    /* receive ignores anything that isn't a valid packet */
    const uint8_t junk[] = { 0x08, 0x01, 0xC0, 0x00, 0x00, 0x05, 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00 };
    send_raw(junk, sizeof(junk));
    check("junk datagram is ignored", radio_receive(got, RADIO_MAX_PAYLOAD) == 0);

    /* bad input */
    static uint8_t too_big[RADIO_QUEUE_SIZE + 1];
    check("send bigger than the tx buffer rejected", radio_send(too_big, sizeof(too_big)) == -1);
    check("rejected send queued nothing", radio_queue_pending() == 0);
    check("send rejects NULL", radio_send(NULL, 10) == -1);
    check("send rejects 0 bytes", radio_send(sent, 0) == -1);

    check("send 100 bytes", radio_send(sent, 100) == 0);
    check("receive rejects buffer too small for the payload", radio_receive(got, 50) == -1);

    /* counters */
    radio_stats_t stats;
    radio_get_stats(&stats);
    check("sent counter counts every packet", stats.packets_sent == 12);
    check("resend counter counts the arq resend", stats.packets_resent == 1);
    check("nothing dropped or retried", stats.packets_dropped == 0 && stats.packets_retried == 0);
    check("rejected sends are counted", stats.queue_rejects == 4);
    check("received counter counts valid packets", stats.rx_packets == 12);
    check("junk shows up in the rx error counters", stats.rx_bytes_skipped > 0);
    check("high water mark recorded", stats.queue_high_water > 0);

    if (failures != 0) {
        printf("radio_sim_test: %d FAILED\n", failures);
        return 1;
    }
    printf("radio_sim_test: PASS\n");
    return 0;
}
