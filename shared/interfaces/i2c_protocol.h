#ifndef I2C_PROTOCOL_H
#define I2C_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#include "frame.h"

/*
Commands written by the I2C master before the associated operation.
*/
typedef enum {
    I2C_CMD_GET_STATUS = 0x01,
    I2C_CMD_READ_FRAME = 0x02,
    I2C_CMD_WRITE_FRAME = 0x03
} I2cCommand_t;

/*
Identifies a valid status response and its protocol version.
This prevents an uninitialized 0xFF-filled response from being mistaken
for a real frame length.
*/

#define I2C_PROTOCOL_MAGIC 0xA5U
#define I2C_PROTOCOL_VERSION 0x01U

/*
Status wire format
byte 0: magic
byte 1: protocol version
byte 2: flags
byte 3: pending frame length, high byte
byte 4: pending frame length, low byte
*/

#define I2C_STATUS_WIRE_SIZE 5U
#define I2C_STATUS_DATA_READY 0x01U

typedef struct {
    uint8_t flags;

    /*
    Total serialized frame length, including the four-byte Frame header.
    This is zero when no data is ready.
    */
    uint16_t frame_length;
} I2cStatus_t;

/*
Encodes a status structure into a 5 byte representation.
Returns 0 on success and -1 for invalid arguments or inconsistent fields.
*/
int i2c_status_encode(
    const I2cStatus_t *status,
    uint8_t *wire_buffer,
    size_t wire_buffer_size
);

/*
validates and decodes a five byte status response.
return 0 on success and -1 for malformed or unsupported responses.
*/
int i2c_status_decode(
    const uint8_t *wire_buffer,
    size_t wire_length,
    I2cStatus_t *status_out
);

#endif