#include "obc_radio_protocol.h"

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "obc_log.h"

#if defined(__linux__)
#include <pigpiod_if2.h>
#endif

static volatile sig_atomic_t running = 1;

static void request_stop(int signal_number)
{
    (void)signal_number;
    running = 0;
}

static void release_soft_rx(void)
{
#if defined(__linux__)
    const char *swapped = getenv("E22_SWAPPED_UART");
    if (swapped != NULL && strcmp(swapped, "1") == 0) {
        int pi = pigpio_start(NULL, NULL);
        if (pi >= 0) {
            int rc = bb_serial_read_close(pi, 14);
            if (rc < 0) fprintf(stderr, "[OBC RADIO] GPIO14 reader cleanup: %d\n", rc);
            pigpio_stop(pi);
        }
    }
#endif
}

static void *receive_uplink(void *unused)
{
    (void)unused;
    uint8_t data[OBC_RADIO_MAX_DATA];
    _Alignas(obc_radio_message_t) uint8_t wire[OBC_RADIO_HEADER_SIZE + OBC_RADIO_MAX_DATA];
    obc_radio_message_t *message = (obc_radio_message_t *)wire;

    while (running) {
        int length = radio_receive(data, sizeof(data));
        if (length <= 0) continue;
        message->kind = OBC_RADIO_UPLINK;
        message->status = 0;
        message->length = (uint16_t)length;
        message->request_id = 0;
        memcpy(message->payload, data, (size_t)length);
        if (IPC_send(ROLE_COMMANDS, wire, OBC_RADIO_HEADER_SIZE + (uint16_t)length) < 0)
            fprintf(stderr, "[OBC RADIO] could not forward uplink to Commands\n");
    }
    return NULL;
}

static void *send_heartbeats(void *unused)
{
    (void)unused;
    while (running) {
        IPC_send(ROLE_FDIR, NULL, 0);
        IPC_send(ROLE_SUPERVISOR, NULL, 0);
        sleep(1);
    }
    return NULL;
}

static int transmit(const obc_radio_message_t *message)
{
    radio_stats_t before, current;
    radio_get_stats(&before);
    int queued = message->kind == OBC_RADIO_SEND_SSDV
        ? radio_send_ssdv(message->payload, message->length)
        : radio_send(message->payload, message->length);
    if (queued != 0) return -1;

    /* The driver queues asynchronously. Report success only after its worker
       has written this one frame to the E22 UART. This does not prove RF delivery. */
    for (int elapsed = 0; elapsed < 15000 && running; elapsed += 10) {
        radio_get_stats(&current);
        if (current.packets_dropped != before.packets_dropped) return -1;
        if (current.packets_sent != before.packets_sent) return 0;
        struct timespec pause = { .tv_sec = 0, .tv_nsec = 10000000L };
        nanosleep(&pause, NULL);
    }
    return -1;
}

int main(void)
{
    obc_log_init("radio");
    LOG_INFO("Initializing");
    if (IPC_initialize(ROLE_RADIO) != IPC_OK) return 1;

    struct sigaction stop = {0};
    stop.sa_handler = request_stop;
    sigemptyset(&stop.sa_mask);
    sigaction(SIGTERM, &stop, NULL);
    sigaction(SIGINT, &stop, NULL);
    atexit(release_soft_rx);

    pthread_t rx_thread, heartbeat_thread;
    if (pthread_create(&rx_thread, NULL, receive_uplink, NULL) != 0) return 1;
    if (pthread_create(&heartbeat_thread, NULL, send_heartbeats, NULL) != 0) return 1;

    _Alignas(obc_radio_message_t) uint8_t wire[OBC_IPC_MAX_PAYLOAD];
    while (running) {
        OBC_Roles_t source;
        int length = IPC_receive_timeout(&source, wire, sizeof(wire), 100);
        if (length < (int)OBC_RADIO_HEADER_SIZE) continue;
        obc_radio_message_t *message = (obc_radio_message_t *)wire;
        if ((source != ROLE_COMMANDS && source != ROLE_MISSION) ||
            (message->kind != OBC_RADIO_SEND_RAW && message->kind != OBC_RADIO_SEND_SSDV) ||
            message->length == 0 || message->length > OBC_RADIO_MAX_DATA ||
            length != (int)(OBC_RADIO_HEADER_SIZE + message->length) ||
            (message->kind == OBC_RADIO_SEND_SSDV && message->length != RADIO_MAX_PAYLOAD)) {
            fprintf(stderr, "[OBC RADIO] rejected malformed send request\n");
            continue;
        }

        int result = transmit(message);
        if (result != 0) fprintf(stderr, "[OBC RADIO] local transmit failed\n");
        if (message->request_id != 0) {
            uint32_t id = message->request_id;
            obc_radio_message_t reply = {
                .kind = OBC_RADIO_RESULT,
                .status = result == 0 ? 0 : 1,
                .length = 0,
                .request_id = id
            };
            if (IPC_send(source, (const uint8_t *)&reply, OBC_RADIO_HEADER_SIZE) < 0)
                fprintf(stderr, "[OBC RADIO] could not return transmit result\n");
        }
    }
    return 0;
}
