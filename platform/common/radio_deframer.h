#ifndef RADIO_DEFRAMER_H
#define RADIO_DEFRAMER_H

#include <stddef.h>
#include <stdint.h>

#include "radio.h"

/*
finds whole ccsds packets in a stream of uart bytes.
keeps a partial packet between reads, uses the ccsds length field to know
where a packet ends, and checks the crc before handing it out. if something
doesn't check out it drops one byte and looks again, so it resyncs on its own.
*/

typedef struct {
    uint8_t buf[RADIO_MAX_PACKET_SIZE];
    size_t len;
} radio_deframer_t;

void radio_deframer_reset(radio_deframer_t *d);

/* bytes held for a packet that isn't complete yet */
size_t radio_deframer_buffered(const radio_deframer_t *d);

/* add one received byte. call radio_deframer_next after each push */
void radio_deframer_push(radio_deframer_t *d, uint8_t byte);

/* if a complete valid packet is buffered, copies it into packet (RADIO_MAX_PACKET_SIZE bytes)
   and returns its length. 0 = need more bytes */
size_t radio_deframer_next(radio_deframer_t *d, uint8_t *packet);

#endif
