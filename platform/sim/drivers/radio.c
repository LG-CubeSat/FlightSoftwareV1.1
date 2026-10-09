#include "radio.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

/*
sim e22: each radio packet = one udp datagram on localhost.
udp drops packets if nobody is listening, same as rf.
*/

#define RADIO_SIM_DEFAULT_TX_PORT 5600 // fake ground station listens here
#define RADIO_SIM_DEFAULT_RX_PORT 5601 // we listen here for uplink
#define RADIO_SIM_DEFAULT_RX_TIMEOUT_MS 1000

static pthread_once_t tx_once = PTHREAD_ONCE_INIT;
static pthread_once_t rx_once = PTHREAD_ONCE_INIT;
static int tx_fd = -1;
static int rx_fd = -1;
static struct sockaddr_in tx_addr;
static int drop_percent = 0;

static int env_int(const char *name, int fallback)
{
    const char *value = getenv(name);
    return value != NULL ? atoi(value) : fallback;
}

static void open_tx(void)
{
    tx_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (tx_fd < 0) {
        perror("[RADIO] (sim) tx socket");
        return;
    }

    memset(&tx_addr, 0, sizeof(tx_addr));
    tx_addr.sin_family = AF_INET;
    tx_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    tx_addr.sin_port = htons((uint16_t)env_int("RADIO_SIM_TX_PORT", RADIO_SIM_DEFAULT_TX_PORT));

    drop_percent = env_int("RADIO_SIM_DROP_PERCENT", 0); // fake packet loss for testing
}

static void open_rx(void)
{
    rx_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (rx_fd < 0) {
        perror("[RADIO] (sim) rx socket");
        return;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)env_int("RADIO_SIM_RX_PORT", RADIO_SIM_DEFAULT_RX_PORT));

    if (bind(rx_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[RADIO] (sim) rx bind");
        close(rx_fd);
        rx_fd = -1;
        return;
    }

    int timeout_ms = env_int("RADIO_SIM_RX_TIMEOUT_MS", RADIO_SIM_DEFAULT_RX_TIMEOUT_MS);
    struct timeval timeout = { .tv_sec = timeout_ms / 1000, .tv_usec = (timeout_ms % 1000) * 1000 };
    setsockopt(rx_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
}

int radio_send(const uint8_t *data, size_t length)
{
    if (data == NULL || length == 0) return -1;

    pthread_once(&tx_once, open_tx);
    if (tx_fd < 0) return -1;

    size_t packets = 0;
    for (size_t offset = 0; offset < length; offset += RADIO_MAX_PACKET_SIZE) {
        size_t chunk = length - offset;
        if (chunk > RADIO_MAX_PACKET_SIZE) chunk = RADIO_MAX_PACKET_SIZE;
        packets++;

        if (drop_percent > 0 && rand() % 100 < drop_percent) continue; // lost in the air

        if (sendto(tx_fd, data + offset, chunk, 0, (struct sockaddr *)&tx_addr, sizeof(tx_addr)) < 0) {
            perror("[RADIO] (sim) sendto");
            return -1;
        }
    }

    printf("[RADIO] (sim) transmitted %zu bytes in %zu packets\n", length, packets);
    fflush(stdout);
    return 0;
}

int radio_receive(uint8_t *buffer, uint16_t max_length)
{
    if (buffer == NULL || max_length == 0) return -1;

    pthread_once(&rx_once, open_rx);
    if (rx_fd < 0) return -1;

    uint8_t packet[RADIO_MAX_PACKET_SIZE + 1]; // +1 so oversized datagrams get caught
    ssize_t n = recv(rx_fd, packet, sizeof(packet), 0);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return 0; // timed out, nothing heard
        return -1;
    }
    if (n > RADIO_MAX_PACKET_SIZE || n > max_length) return -1;

    memcpy(buffer, packet, (size_t)n);
    return (int)n;
}
