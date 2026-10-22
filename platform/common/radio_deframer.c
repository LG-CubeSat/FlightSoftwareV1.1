#include "radio_deframer.h"

#include <string.h>

#include "ccsds/profile.h"
#include "radio_queue.h"

#define FIRST_BYTE_MASK 0xE8 // version (3 bits) + secondary header flag
#define FIRST_BYTE_OK 0x08   // version 0, secondary header present (every lg profile packet has one)

static void drop(radio_deframer_t *d, size_t n)
{
    memmove(d->buf, d->buf + n, d->len - n);
    d->len -= n;
}

void radio_deframer_reset(radio_deframer_t *d)
{
    d->len = 0;
}

size_t radio_deframer_buffered(const radio_deframer_t *d)
{
    return d->len;
}

void radio_deframer_push(radio_deframer_t *d, uint8_t byte)
{
    if (d->len == sizeof(d->buf)) { // only if next wasn't called, keep the newest bytes
        drop(d, 1);
        radio_stats_rx_skipped(1);
    }
    d->buf[d->len++] = byte;
}

size_t radio_deframer_next(radio_deframer_t *d, uint8_t *packet)
{
    for (;;) {
        if (d->len == 0) return 0;

        if ((d->buf[0] & FIRST_BYTE_MASK) != FIRST_BYTE_OK) { // can't be the start of a packet
            drop(d, 1);
            radio_stats_rx_skipped(1);
            continue;
        }
        if (d->len < CCSDS_SPACE_PACKET_PRIMARY_HEADER_SIZE) return 0;

        // ccsds length field = data bytes - 1
        size_t total = CCSDS_SPACE_PACKET_PRIMARY_HEADER_SIZE + (((size_t)d->buf[4] << 8) | d->buf[5]) + 1;
        if (total < RADIO_FRAME_OVERHEAD || total > RADIO_MAX_PACKET_SIZE) { // impossible length, not a real header
            drop(d, 1);
            radio_stats_rx_skipped(1);
            continue;
        }
        if (d->len < total) return 0; // wait for the rest

        lg_ccsds_profile_view_t view;
        if (lg_ccsds_profile_parse(d->buf, total, &view) == CCSDS_OK) {
            memcpy(packet, d->buf, total);
            drop(d, total);
            radio_stats_rx_packet();
            return total;
        }

        // looked like a packet but crc or format failed. slide one byte and look again
        radio_stats_rx_invalid();
        drop(d, 1);
        radio_stats_rx_skipped(1);
    }
}
