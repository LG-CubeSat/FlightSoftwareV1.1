#include "radio_queue.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "radio.h"

#define SEND_ATTEMPTS 3
#define RETRY_DELAY_MS 100
#define MESSAGE_HEADER 5 // content type (1) + length (4) stored in front of each message
#define CONTENT_TYPES 4
#define RAW_FRAME 0xFF   // message type for an already framed packet (a resend), sent as is
#define HISTORY_SIZE 600 // sent packets kept for arq resends, ~2.5 photos worth of ssdv

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t has_data = PTHREAD_COND_INITIALIZER;
static uint8_t buffer[RADIO_QUEUE_SIZE];
static size_t head = 0;      // next byte to read
static size_t count = 0;     // bytes waiting, message headers included
static size_t in_flight = 0; // payload bytes taken out, currently being sent
static int thread_started = 0;
static radio_packet_sender_t sender = NULL;
static radio_stats_t stats;

typedef struct {
    int used;
    uint16_t apid;
    uint16_t sequence;
    size_t length;
    uint8_t packet[RADIO_MAX_PACKET_SIZE];
} sent_packet_t;

static sent_packet_t history[HISTORY_SIZE]; // oldest gets overwritten first
static size_t history_next = 0;

static void sleep_ms(int ms)
{
    struct timespec ts = { .tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

/* ring helpers, caller holds the lock */
static void ring_read(uint8_t *dst, size_t n)
{
    size_t first = RADIO_QUEUE_SIZE - head; // bytes before the buffer wraps around
    if (first > n) first = n;
    memcpy(dst, buffer + head, first);
    memcpy(dst + first, buffer, n - first);
    head = (head + n) % RADIO_QUEUE_SIZE;
    count -= n;
}

static void ring_write(const uint8_t *src, size_t n)
{
    size_t tail = (head + count) % RADIO_QUEUE_SIZE;
    size_t first = RADIO_QUEUE_SIZE - tail;
    if (first > n) first = n;
    memcpy(buffer + tail, src, first);
    memcpy(buffer, src + first, n - first);
    count += n;
}

static void *tx_thread(void *arg)
{
    (void)arg;
    uint8_t chunk[RADIO_MAX_PAYLOAD];
    uint8_t packet[RADIO_MAX_PACKET_SIZE];
    uint16_t sequence[CONTENT_TYPES] = {0}; // ccsds counts packets per apid
    uint8_t content = LG_CCSDS_CONTENT_RAW_TELEMETRY;
    size_t message_left = 0;

    for (;;) {
        pthread_mutex_lock(&lock);
        while (count == 0) pthread_cond_wait(&has_data, &lock);

        if (message_left == 0) { // start of the next message
            uint8_t header[MESSAGE_HEADER];
            ring_read(header, sizeof(header));
            content = header[0];
            message_left = (size_t)header[1] << 24 | (size_t)header[2] << 16 |
                           (size_t)header[3] << 8 | header[4];
        }

        size_t packet_length = 0;
        size_t n;
        uint16_t seq = 0;
        int framed = 0;

        if (content == RAW_FRAME) { // resend: already a full packet, send it exactly as before
            n = message_left;
            ring_read(packet, n);
            message_left = 0;
            packet_length = n;
            seq = (uint16_t)(((packet[2] & 0x3F) << 8) | packet[3]);
            framed = 1;
        } else {
            // ssdv messages are multiples of 214, so every ssdv chunk is exactly one ssdv packet
            n = message_left < RADIO_MAX_PAYLOAD ? message_left : RADIO_MAX_PAYLOAD;
            ring_read(chunk, n);
            message_left -= n;
            seq = sequence[content];
            sequence[content] = (uint16_t)((seq + 1U) & CCSDS_SPACE_PACKET_MAX_SEQUENCE_COUNT);
        }
        in_flight = n;
        radio_packet_sender_t send_packet = sender;
        pthread_mutex_unlock(&lock);

        int sent = -1;
        int attempts = 0;
        if (!framed && lg_ccsds_profile_build((lg_ccsds_content_type_t)content, seq, 1, NULL, chunk, n,
                                              packet, sizeof(packet), &packet_length) != CCSDS_OK) {
            fprintf(stderr, "[RADIO] couldn't frame %zu bytes\n", n);
        } else {
            // send outside the lock so radio_send never waits on the radio
            while (attempts < SEND_ATTEMPTS && sent != 0) {
                if (attempts > 0) sleep_ms(RETRY_DELAY_MS);
                sent = send_packet(packet, packet_length);
                attempts++;
            }
        }
        if (sent != 0) {
            fprintf(stderr, "[RADIO] dropped packet seq %u (%zu bytes) after %d attempts\n",
                    (unsigned)seq, n, attempts);
        }

        pthread_mutex_lock(&lock);
        in_flight = 0;
        if (attempts > 1) stats.packets_retried += (uint32_t)(attempts - 1);
        if (sent == 0) {
            stats.packets_sent++;
            if (framed) {
                stats.packets_resent++;
            } else { // remember it in case the ground asks for it again
                sent_packet_t *slot = &history[history_next];
                history_next = (history_next + 1) % HISTORY_SIZE;
                slot->used = 1;
                slot->apid = (uint16_t)(((packet[0] & 0x07) << 8) | packet[1]);
                slot->sequence = seq;
                slot->length = packet_length;
                memcpy(slot->packet, packet, packet_length);
            }
        } else {
            stats.packets_dropped++;
        }
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

/* caller holds the lock */
static int push_locked(uint8_t content, const uint8_t *data, size_t length)
{
    if (length + MESSAGE_HEADER > RADIO_QUEUE_SIZE - count) return -1;

    uint8_t header[MESSAGE_HEADER] = {
        content,
        (uint8_t)(length >> 24), (uint8_t)(length >> 16), (uint8_t)(length >> 8), (uint8_t)length
    };
    ring_write(header, sizeof(header));
    ring_write(data, length);
    if (count > stats.queue_high_water) stats.queue_high_water = (uint32_t)count;
    pthread_cond_signal(&has_data);
    return 0;
}

int radio_queue_push(lg_ccsds_content_type_t content, const uint8_t *data, size_t length,
                     radio_packet_sender_t send_packet)
{
    int bad_args = data == NULL || length == 0 || length > UINT32_MAX || send_packet == NULL ||
                   (unsigned)content >= CONTENT_TYPES ||
                   (content == LG_CCSDS_CONTENT_SSDV && length % LG_CCSDS_SSDV_PACKET_SIZE != 0);

    pthread_mutex_lock(&lock);

    if (bad_args) {
        stats.queue_rejects++;
        pthread_mutex_unlock(&lock);
        return -1;
    }

    if (!thread_started) {
        pthread_t tid;
        sender = send_packet;
        if (pthread_create(&tid, NULL, tx_thread, NULL) != 0) {
            stats.queue_rejects++;
            pthread_mutex_unlock(&lock);
            fprintf(stderr, "[RADIO] tx thread failed to start\n");
            return -1;
        }
        pthread_detach(tid);
        thread_started = 1;
    }

    if (push_locked((uint8_t)content, data, length) != 0) { // all or nothing, never queue half a message
        stats.queue_rejects++;
        pthread_mutex_unlock(&lock);
        fprintf(stderr, "[RADIO] tx buffer full, %zu bytes not queued\n", length);
        return -1;
    }

    pthread_mutex_unlock(&lock);
    return 0;
}

int radio_resend(uint16_t apid, uint16_t sequence_count)
{
    pthread_mutex_lock(&lock);

    // newest match wins, in case the 14 bit sequence count has wrapped
    for (size_t i = 0; i < HISTORY_SIZE; i++) {
        size_t index = (history_next + HISTORY_SIZE - 1 - i) % HISTORY_SIZE;
        sent_packet_t *slot = &history[index];
        if (slot->used && slot->apid == apid && slot->sequence == sequence_count) {
            int result = thread_started ? push_locked(RAW_FRAME, slot->packet, slot->length) : -1;
            pthread_mutex_unlock(&lock);
            return result;
        }
    }

    pthread_mutex_unlock(&lock);
    return -1; // too old or never sent
}

size_t radio_queue_pending(void)
{
    pthread_mutex_lock(&lock);
    size_t pending = count + in_flight;
    pthread_mutex_unlock(&lock);
    return pending;
}

void radio_stats_rx_packet(void)
{
    pthread_mutex_lock(&lock);
    stats.rx_packets++;
    pthread_mutex_unlock(&lock);
}

void radio_stats_rx_invalid(void)
{
    pthread_mutex_lock(&lock);
    stats.rx_invalid++;
    pthread_mutex_unlock(&lock);
}

void radio_stats_rx_skipped(size_t bytes)
{
    pthread_mutex_lock(&lock);
    stats.rx_bytes_skipped += (uint32_t)bytes;
    pthread_mutex_unlock(&lock);
}

void radio_get_stats(radio_stats_t *out)
{
    if (out == NULL) return;
    pthread_mutex_lock(&lock);
    *out = stats;
    pthread_mutex_unlock(&lock);
}
