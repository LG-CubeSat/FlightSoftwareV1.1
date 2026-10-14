#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "radio.h"
#include "radio_queue.h"

#define TEST_PORT "56240" // tx + rx on the same port = loopback

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

int main(void)
{
    setenv("RADIO_SIM_TX_PORT", TEST_PORT, 1);
    setenv("RADIO_SIM_RX_PORT", TEST_PORT, 1);
    setenv("RADIO_SIM_RX_TIMEOUT_MS", "200", 1);

    uint8_t sent[600];
    uint8_t got[RADIO_MAX_PACKET_SIZE];

    check("receive with nothing sent times out", radio_receive(got, sizeof(got)) == 0);

    fill(sent, RADIO_MAX_PACKET_SIZE, 1);
    check("send one full 240 byte packet", radio_send(sent, RADIO_MAX_PACKET_SIZE) == 0);
    int n = radio_receive(got, sizeof(got));
    check("full packet arrives whole", n == RADIO_MAX_PACKET_SIZE && memcmp(got, sent, (size_t)n) == 0);

    fill(sent, 500, 7);
    check("send 500 bytes", radio_send(sent, 500) == 0);
    int a = radio_receive(got, sizeof(got));
    int a_ok = a == 240 && memcmp(got, sent, 240) == 0;
    int b = radio_receive(got, sizeof(got));
    int b_ok = b == 240 && memcmp(got, sent + 240, 240) == 0;
    int c = radio_receive(got, sizeof(got));
    int c_ok = c == 20 && memcmp(got, sent + 480, 20) == 0;
    check("500 bytes split into 240 + 240 + 20", a_ok && b_ok && c_ok);

    /* sizes from the pr #14 review: smallest, one ssdv packet, and one over the limit */
    fill(sent, 1, 3);
    check("send 1 byte", radio_send(sent, 1) == 0);
    n = radio_receive(got, sizeof(got));
    check("1 byte arrives as its own packet", n == 1 && got[0] == sent[0]);

    fill(sent, 214, 9);
    check("send one 214 byte ssdv packet", radio_send(sent, 214) == 0);
    n = radio_receive(got, sizeof(got));
    check("214 bytes arrive whole", n == 214 && memcmp(got, sent, 214) == 0);

    fill(sent, 241, 11);
    check("send 241 bytes", radio_send(sent, 241) == 0);
    a = radio_receive(got, sizeof(got));
    a_ok = a == 240 && memcmp(got, sent, 240) == 0;
    b = radio_receive(got, sizeof(got));
    b_ok = b == 1 && got[0] == sent[240];
    check("241 bytes split into 240 + 1", a_ok && b_ok);

    /* the tx buffer is one continuous stream: two quick sends may go out as
       one packet or two, but every byte must arrive once and in order */
    uint8_t first[10];
    uint8_t second[20];
    uint8_t stream[30];
    size_t streamed = 0;
    fill(first, sizeof(first), 20);
    fill(second, sizeof(second), 40);
    radio_send(first, sizeof(first));
    radio_send(second, sizeof(second));
    while (streamed < sizeof(stream)) {
        n = radio_receive(got, sizeof(got));
        if (n <= 0 || streamed + (size_t)n > sizeof(stream)) break;
        memcpy(stream + streamed, got, (size_t)n);
        streamed += (size_t)n;
    }
    check("back to back sends arrive in order as one stream",
          streamed == 30 && memcmp(stream, first, 10) == 0 && memcmp(stream + 10, second, 20) == 0);
    check("nothing left over after back to back", radio_receive(got, sizeof(got)) == 0);

    /* more than the whole buffer can never fit, so nothing gets queued */
    static uint8_t too_big[RADIO_QUEUE_SIZE + 1];
    check("send bigger than the tx buffer rejected", radio_send(too_big, sizeof(too_big)) == -1);
    check("rejected send queued nothing", radio_queue_pending() == 0);

    check("send rejects NULL", radio_send(NULL, 10) == -1);
    check("send rejects 0 bytes", radio_send(sent, 0) == -1);

    check("send 100 bytes", radio_send(sent, 100) == 0);
    check("receive rejects buffer too small for packet", radio_receive(got, 50) == -1);

    if (failures != 0) {
        printf("radio_sim_test: %d FAILED\n", failures);
        return 1;
    }
    printf("radio_sim_test: PASS\n");
    return 0;
}
