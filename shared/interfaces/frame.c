#include "frame.h"

#include <stdio.h>
#include <string.h>
#include <arpa/inet.h> // htons/ntohs

// parses frames into sendable bytes. Uses big endian.
int frame_serialize(const Frame *frame, uint8_t *out_buf, size_t out_buf_size)
{
    // We are doing +4 because dest_addr, src_addr, and length (2 bytes) means a four byte header
    if (frame->length + 4 > out_buf_size) {
        printf("[COMMS BUS] Payload size of %d exceeds limit of %zu", frame->length, out_buf_size);
        return -1;
    }

    out_buf[0] = frame->dest_addr; // set the destination
    out_buf[1] = frame->src_addr; // set where it came from

    uint16_t net_length = htons(frame->length); // convert to big endian
    memcpy(&out_buf[2], &net_length, sizeof(net_length)); // set the length -- always 2 bytes, not frame->length

    memcpy(&out_buf[4], frame->payload, frame->length); // set the actual message/payload -- starts after the 4-byte header

    return 4 + frame->length; // used in the write
}

int frame_deserialize(const uint8_t *wire_buf, int wire_len, Frame *frame_out)
{
    if (wire_len < 4) {
        printf("[COMMS BUS] Received %d bytes, too short to contain a 4-byte header\n", wire_len);
        return -1;
    }

    frame_out->dest_addr = wire_buf[0];
    frame_out->src_addr = wire_buf[1];

    uint16_t net_length;
    memcpy(&net_length, &wire_buf[2], sizeof(net_length)); // pull the 2 length bytes out as-is
    frame_out->length = ntohs(net_length); // then convert back from wire byte order

    if (frame_out->length > MAX_FRAME_PAYLOAD || frame_out->length > (uint16_t)(wire_len - 4)) {
        printf("[COMMS BUS] Frame claims payload length %d, exceeds limit or bytes actually received\n", frame_out->length);
        return -1;
    }

    memcpy(frame_out->payload, &wire_buf[4], frame_out->length); // payload starts after the 4-byte header

    return 4 + frame_out->length;
}
