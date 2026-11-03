/*
 * Manual Pi <-> USB E22 integration test. This links the production platform
 * library and exercises radio_send() and radio_receive(), including CCSDS
 * framing, E22 configuration, GPIO mode pins, and the selected UART backend.
 *
 * On the Pi: E22_SWAPPED_UART=1 ./build-hw/bin/radio_hardware_test
 * On the Mac, run tests/radio_hardware_peer.py first to echo the RF frame.
 */

#include "radio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define SEND_TIMEOUT_MS 12000
#define ECHO_TIMEOUT_MS 15000

static long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static void sleep_ms(int milliseconds)
{
    struct timespec ts = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (milliseconds % 1000) * 1000000L
    };
    nanosleep(&ts, NULL);
}

int main(void)
{
    const char *mode = getenv("E22_SWAPPED_UART");
    if (mode == NULL || (strcmp(mode, "0") != 0 && strcmp(mode, "1") != 0)) {
        fprintf(stderr, "Set E22_SWAPPED_UART=1 for the swapped PCB, or 0 after repair.\n");
        return 2;
    }

    char payload[96];
    int payload_length = snprintf(payload, sizeof(payload),
                                  "E22_DRIVER_TEST pid=%ld tick=%ld",
                                  (long)getpid(), now_ms());
    if (payload_length <= 0 || (size_t)payload_length >= sizeof(payload)) return 2;

    printf("Testing production radio driver with E22_SWAPPED_UART=%s\n", mode);
    printf("Payload: %s\n", payload);
    fflush(stdout);

    radio_stats_t before;
    radio_stats_t after;
    radio_get_stats(&before);

    if (radio_send((const uint8_t *)payload, (size_t)payload_length) != 0) {
        fprintf(stderr, "FAIL: radio_send() could not initialize or queue the frame.\n");
        return 1;
    }

    long deadline = now_ms() + SEND_TIMEOUT_MS;
    for (;;) {
        radio_get_stats(&after);
        if (after.packets_sent > before.packets_sent) break;
        if (after.packets_dropped > before.packets_dropped || now_ms() >= deadline) {
            fprintf(stderr, "FAIL: the radio TX worker did not send the frame.\n");
            return 1;
        }
        sleep_ms(20);
    }

    printf("TX passed: production driver sent one CCSDS frame. Waiting for USB E22 echo...\n");
    fflush(stdout);

    deadline = now_ms() + ECHO_TIMEOUT_MS;
    while (now_ms() < deadline) {
        uint8_t received[RADIO_MAX_PAYLOAD];
        int count = radio_receive(received, sizeof(received));
        if (count < 0) {
            fprintf(stderr, "FAIL: radio_receive() reported an error.\n");
            return 1;
        }
        if (count == payload_length && memcmp(received, payload, (size_t)count) == 0) {
            printf("PASS: production driver sent and received the same payload over the E22 link.\n");
            return 0;
        }
        if (count > 0) {
            fprintf(stderr, "Ignored another valid radio payload (%d bytes).\n", count);
        }
    }

    fprintf(stderr, "FAIL: no matching echo arrived within %d ms.\n", ECHO_TIMEOUT_MS);
    return 1;
}
