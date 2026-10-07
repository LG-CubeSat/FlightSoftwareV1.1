#include "i2c_protocol.h"

static int status_fields_valid(const I2cStatus_t *status)
{
    if (status == NULL) {
        return 0;
    }

    /*
    Reject currently undefined flag bits. A future protocol version can introduce more flags.
    */
    if ((status->flags & (uint8_t)~I2C_STATUS_DATA_RELAY) != 0U) {
        return 0;
    }

    if ((status->flags & I2C_STATUS_DATA_RELAY) == 0U) {
        /*
        No data means there must not be a pending frame length.
        */
        return status->frame_length == 0U;
    }

    /*
    A ready frame must contain at least the four-bytes Frame header and cannot exceed the largest frame our codec supports.
    */
    return status->frame_length >= FRAME_HEADER_SIZE &&
        status->frame_length <= MAX_FRAME_WIRE_SIZE;
}

int i2c_status_encode(
    const I2cStatus_t *status,
    uint8_t *wire_buffer,
    size_t wire_buffer_size
) 
{
    if (wire_buffer == NULL ||
        wire_buffer_size < I2C_STATUS_WIRE_SIZE ||
        !status_fields_valid(status)) {
        return -1;
    }

    wire_buffer[0] = I2C_PROTOCOL_MAGIC;
    wire_buffer[1] = I2C_PROTOCOL_VERSION;
    wire_buffer[2] = status->flags;

    /*
    encode the 16 bit length in big endian/network byte order.
    Doing this manually keeps the protocol portable to bare-metal STM32.
    */
    wire_buffer[3] = (uint8_t)(status->frame_length >> 8);
    wire_buffer[4] = (uint8_t)(status->frame_length & 0xFFU);

    return 0;
}

int i2c_status_decode(
    const uint8_t *wire_buffer,
    size_t wire_length,
    I2cStatus_t *status_out
)
{
    if (wire_buffer == NULL ||
        status_out == NULL ||
        wire_length != I2C_STATUS_WIRE_SIZE
    ) {
        return -1;
    }

    if (wire_buffer[0] != I2C_PROTOCOL_MAGIC || wire_buffer[1] != I2C_PROTOCOL_VERSION) {
        return -1;
    }

    I2cStatus_t decoded = {
        .flags = wire_buffer[2],
        .frame_length = (
            (uint16_t)wire_buffer[3] << 8 |
            (uint16_t)wire_buffer[4]
        )
    };

    if (!status_fields_valid(&decoded)) {
        return -1;
    }

    *status_out = decoded;
    return 0;
} 