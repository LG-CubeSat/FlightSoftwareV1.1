#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "radio.h"

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
