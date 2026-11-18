#include "frame.h"

#include <string.h>

// parses frames into sendable bytes. Uses big endian.
int frame_serialize(const Frame *frame, uint8_t *out_buf, size_t out_buf_size)
{
    if (frame == NULL || out_buf == NULL) {
        return -1;
    }

    if (frame->length > MAX_FRAME_PAYLOAD) {
        return -1;
    }

    // We are doing +4 because dest_addr, src_addr, and length (2 bytes) means a four byte header
    if ((size_t)frame->length + FRAME_HEADER_SIZE > out_buf_size) {
        return -1;
    }

    out_buf[0] = frame->dest_addr; // set the destination
    out_buf[1] = frame->src_addr; // set where it came from

    /* Encode explicitly so this codec also builds on bare-metal targets. */
    out_buf[2] = (uint8_t)(frame->length >> 8U);
    out_buf[3] = (uint8_t)(frame->length & 0xFFU);

    memcpy(&out_buf[4], frame->payload, frame->length); // set the actual message/payload -- starts after the 4-byte header

    return (int)(FRAME_HEADER_SIZE + frame->length); // used in the write
}

int frame_deserialize(const uint8_t *wire_buf, int wire_len, Frame *frame_out)
{
    if (wire_buf == NULL || frame_out == NULL || wire_len < (int)FRAME_HEADER_SIZE) {
        return -1;
    }

    frame_out->dest_addr = wire_buf[0];
    frame_out->src_addr = wire_buf[1];

    frame_out->length = (uint16_t)(
        ((uint16_t)wire_buf[2] << 8U) |
        wire_buf[3]
    );

    if (frame_out->length > MAX_FRAME_PAYLOAD ||
        wire_len != (int)(FRAME_HEADER_SIZE + frame_out->length)) {
        return -1;
    }

    memcpy(frame_out->payload, &wire_buf[4], frame_out->length); // payload starts after the 4-byte header

    return (int)(FRAME_HEADER_SIZE + frame_out->length);
}
