#ifndef ADCS_I2C_SLAVE_H
#define ADCS_I2C_SLAVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "stm32g4xx_hal.h"

/* Seven-bit bus address. The STM32 HAL stores it shifted left by one bit. */
#define ADCS_I2C_7BIT_ADDRESS       0x2AU
#define ADCS_I2C_HAL_OWN_ADDRESS    (ADCS_I2C_7BIT_ADDRESS << 1U)

#define ADCS_I2C_PROTOCOL_MAGIC     0xA5U
#define ADCS_I2C_PROTOCOL_VERSION   0x01U
#define ADCS_I2C_STATUS_WIRE_SIZE   5U
#define ADCS_I2C_STATUS_DATA_READY  0x01U

#define ADCS_I2C_CMD_GET_STATUS     0x01U
#define ADCS_I2C_CMD_READ_FRAME     0x02U
#define ADCS_I2C_CMD_WRITE_FRAME    0x03U

#define ADCS_I2C_FRAME_HEADER_SIZE       4U
#define ADCS_I2C_MAX_FRAME_PAYLOAD       264U
#define ADCS_I2C_MAX_FRAME_WIRE_SIZE \
    (ADCS_I2C_FRAME_HEADER_SIZE + ADCS_I2C_MAX_FRAME_PAYLOAD)

/* Event bits are set by callbacks and consumed by the main loop. */
#define ADCS_I2C_EVENT_ACTIVITY          (1UL << 0)
#define ADCS_I2C_EVENT_RX_FRAME_READY    (1UL << 1)
#define ADCS_I2C_EVENT_INVALID_COMMAND   (1UL << 2)
#define ADCS_I2C_EVENT_MALFORMED_FRAME   (1UL << 3)
#define ADCS_I2C_EVENT_ERROR             (1UL << 4)
#define ADCS_I2C_EVENT_RECOVERED         (1UL << 5)
#define ADCS_I2C_EVENT_RX_OVERRUN        (1UL << 6)

typedef struct
{
    uint16_t length;
    uint8_t bytes[ADCS_I2C_MAX_FRAME_WIRE_SIZE];
} adcs_i2c_frame_t;

typedef struct
{
    uint32_t address_match_count;
    uint32_t receive_start_count;
    uint32_t receive_complete_count;
    uint32_t transmit_start_count;
    uint32_t transmit_complete_count;
    uint32_t listen_complete_count;
    uint32_t error_count;
    uint32_t callback_error_count;
    uint32_t invalid_command_count;
    uint32_t malformed_frame_count;
    uint32_t recovery_count;
    uint32_t receive_overrun_count;
    uint32_t last_error;
    uint16_t last_address_match;
    uint8_t last_direction;
    uint8_t last_command;
} adcs_i2c_stats_t;

/* The supplied HAL handle must already have been initialized. */
HAL_StatusTypeDef adcs_i2c_slave_init(I2C_HandleTypeDef *i2c);

/* Run non-blocking recovery work from the main loop. */
void adcs_i2c_slave_process(void);

/* Atomically consume accumulated event bits and snapshot counters. */
uint32_t adcs_i2c_slave_take_events(void);
void adcs_i2c_slave_get_stats(adcs_i2c_stats_t *stats_out);

/* Move validated wire-format frames between the I2C driver and application. */
bool adcs_i2c_slave_take_received_frame(adcs_i2c_frame_t *frame_out);
HAL_StatusTypeDef adcs_i2c_slave_queue_transmit_frame(
    const uint8_t *wire_frame,
    size_t wire_length
);

#endif
