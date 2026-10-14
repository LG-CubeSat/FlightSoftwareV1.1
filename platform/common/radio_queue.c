#include "radio_queue.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "radio.h"

#define SEND_ATTEMPTS 3
#define RETRY_DELAY_MS 100

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t has_data = PTHREAD_COND_INITIALIZER;
static uint8_t buffer[RADIO_QUEUE_SIZE];
static size_t head = 0;      // next byte to send
static size_t count = 0;     // bytes waiting in the buffer
static size_t in_flight = 0; // bytes taken out, currently being sent
static int thread_started = 0;
static radio_packet_sender_t sender = NULL;

static void sleep_ms(int ms)
{
    struct timespec ts = { .tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

static void *tx_thread(void *arg)
{
    (void)arg;
    uint8_t chunk[RADIO_MAX_PACKET_SIZE];

    for (;;) {
        pthread_mutex_lock(&lock);
        while (count == 0) pthread_cond_wait(&has_data, &lock);

        size_t n = count < RADIO_MAX_PACKET_SIZE ? count : RADIO_MAX_PACKET_SIZE;
        size_t first = RADIO_QUEUE_SIZE - head; // bytes before the buffer wraps around
        if (first > n) first = n;
        memcpy(chunk, buffer + head, first);
        memcpy(chunk + first, buffer, n - first);
        head = (head + n) % RADIO_QUEUE_SIZE;
        count -= n;
        in_flight = n;
        radio_packet_sender_t send_packet = sender;
        pthread_mutex_unlock(&lock);

        // send outside the lock so radio_send never waits on the radio
        int sent = -1;
        for (int attempt = 0; attempt < SEND_ATTEMPTS && sent != 0; attempt++) {
            if (attempt > 0) sleep_ms(RETRY_DELAY_MS);
            sent = send_packet(chunk, n);
        }
        if (sent != 0) {
            fprintf(stderr, "[RADIO] dropped %zu bytes after %d failed sends\n", n, SEND_ATTEMPTS);
        }

        pthread_mutex_lock(&lock);
        in_flight = 0;
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

int radio_queue_push(const uint8_t *data, size_t length, radio_packet_sender_t send_packet)
{
    if (data == NULL || length == 0 || send_packet == NULL) return -1;

    pthread_mutex_lock(&lock);

    if (!thread_started) {
        pthread_t tid;
        sender = send_packet;
        if (pthread_create(&tid, NULL, tx_thread, NULL) != 0) {
            pthread_mutex_unlock(&lock);
            fprintf(stderr, "[RADIO] tx thread failed to start\n");
            return -1;
        }
        pthread_detach(tid);
        thread_started = 1;
    }

    if (length > RADIO_QUEUE_SIZE - count) { // all or nothing, never queue half a message
        pthread_mutex_unlock(&lock);
        fprintf(stderr, "[RADIO] tx buffer full, %zu bytes not queued\n", length);
        return -1;
    }

    size_t tail = (head + count) % RADIO_QUEUE_SIZE;
    size_t first = RADIO_QUEUE_SIZE - tail;
    if (first > length) first = length;
    memcpy(buffer + tail, data, first);
    memcpy(buffer, data + first, length - first);
    count += length;

    pthread_cond_signal(&has_data);
    pthread_mutex_unlock(&lock);
    return 0;
}

size_t radio_queue_pending(void)
{
    pthread_mutex_lock(&lock);
    size_t pending = count + in_flight;
    pthread_mutex_unlock(&lock);
    return pending;
}
