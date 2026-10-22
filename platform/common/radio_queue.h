#ifndef RADIO_QUEUE_H
#define RADIO_QUEUE_H

#include <stddef.h>
#include <stdint.h>

#include "ccsds/profile.h"

/*
tx ring buffer shared by the sim and real radio drivers.
radio_send copies into the buffer and returns right away. a background
thread takes up to RADIO_MAX_PAYLOAD bytes at a time, wraps them in a ccsds
space packet (lg profile) and hands the <= 240 byte packet to the driver,
so a long photo downlink doesn't block mission.
*/

#define RADIO_QUEUE_SIZE (128 * 1024) // fits a full compressed photo plus telemetry

/* driver function that puts one framed packet (<= RADIO_MAX_PACKET_SIZE) on the air. 0 = sent */
typedef int (*radio_packet_sender_t)(const uint8_t *data, size_t length);

/* 0 = queued. -1 = bad args, ssdv length not a multiple of 214, or not enough room (nothing queued) */
int radio_queue_push(lg_ccsds_content_type_t content, const uint8_t *data, size_t length,
                     radio_packet_sender_t send_packet);

/* bytes not sent yet, including the packet currently going out */
size_t radio_queue_pending(void);

/* receive side counters, called by the drivers and the deframer */
void radio_stats_rx_packet(void);
void radio_stats_rx_invalid(void);
void radio_stats_rx_skipped(size_t bytes);

#endif
