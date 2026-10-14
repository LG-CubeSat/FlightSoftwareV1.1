#ifndef RADIO_QUEUE_H
#define RADIO_QUEUE_H

#include <stddef.h>
#include <stdint.h>

/*
tx ring buffer shared by the sim and real radio drivers.
radio_send copies into the buffer and returns right away. a background
thread keeps handing the next <= RADIO_MAX_PACKET_SIZE bytes to the driver,
so a long photo downlink doesn't block mission.
*/

#define RADIO_QUEUE_SIZE (128 * 1024) // fits a full compressed photo plus telemetry

/* driver function that puts one packet (<= RADIO_MAX_PACKET_SIZE) on the air. 0 = sent */
typedef int (*radio_packet_sender_t)(const uint8_t *data, size_t length);

/* 0 = queued. -1 = bad args or not enough room, and nothing is queued */
int radio_queue_push(const uint8_t *data, size_t length, radio_packet_sender_t send_packet);

/* bytes not sent yet, including the packet currently going out */
size_t radio_queue_pending(void);

#endif
