#include "comms_bus.h"
#include "frame.h"
#include "csp_commands.h" // has the csp addresses
#include "i2c_addresses.h" // has the addresses for i2c
#include "i2c_protocol.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>

/* The following may be red/error because you are not on a Linux device. */
#include <linux/i2c-dev.h> // I2C_RDWR, I2C_M_RD
#include <linux/i2c.h> // struct i2c_msg, struct i2c_rdwer_ioctl_data

#define I2C_BUS_PATH "/dev/i2c-1"
#define I2C_WRITE_BUFFER_SIZE (1U + MAX_FRAME_WIRE_SIZE) // fixed-size read block, see receive() below

typedef struct {
    uint8_t csp_addr;
    uint8_t i2c_addr;
    const char *name; // for logging only
} known_slave_t;

static const known_slave_t known_slaves[] = {
    { ADCS_ADDRESS, ADCS_I2C_ADDR, "ADCS " },
    { THERMALS_ADDRESS , THERMALS_I2C_ADDR, "THERMALS" }
};

#define NUM_KNOWN_SLAVES (sizeof(known_slaves) / sizeof(known_slaves[0]))
#define POLL_SLEEP_US 20000

static int bus_fd = -1;
static uint8_t my_bus_address;
static size_t next_slave_start = 0; // rotates who gets polled first, see receive function

static int i2c_transfer(
    struct i2c_msg *messages,
    uint32_t message_count
) {
    struct i2c_rdwr_ioctl_data transfer = {
        .msgs = messages,
        .nmsgs = message_count,
    };

    int result;

    do {
        result = ioctl(bus_fd, I2C_RDWR, &transfer);
    } while (result < 0 && errno ==EINTR);

    if (result < 0) {
        return COMMS_BUS_ERROR;
    }

    /*
    I2C_RDWR returns the number of messages executed, not the number of payload bytes transferred.
    */
    if ((uint32_t)result != message_count) {
        return COMMS_BUS_ERROR;
    }

    return COMMS_BUS_OK;
}

CommsBus_t create_comms_bus(void)
{
    CommsBus_t bus;
    bus.initialize = &comms_bus_initialize;
    bus.send = &comms_bus_send;
    bus.receive = &comms_bus_receive;
    return bus;
}

CommsBusStatus_t comms_bus_initialize(uint8_t my_address, int is_master)
{
    my_bus_address = my_address;

    if (!is_master) {
        fprintf(stderr, "[COMMS BUS] comms_i2c_obc only support being master.\n");
        return COMMS_BUS_ERROR;
    }

    bus_fd = open(I2C_BUS_PATH, O_RDWR); // read/write
    if (bus_fd < 0) {
        fprintf(stderr, "[COMMS BUS] open(%s) failed: %s\n", I2C_BUS_PATH, strerror(errno));
        return COMMS_BUS_ERROR;
    }
    
    printf("[COMMS BUS] OBC I2C master ready on %s, address 0x%02x\n", I2C_BUS_PATH, my_bus_address);
    return COMMS_BUS_OK;
}

static int lookup_i2c_addr(uint8_t csp_addr, uint8_t *i2c_addr_out)
{
    for (size_t i = 0; i < NUM_KNOWN_SLAVES; i++) {
        if (known_slaves[i].csp_addr == csp_addr) {
            *i2c_addr_out = known_slaves[i].i2c_addr;
            return 0;
        }
    }
    return -1;
}

// one i2c msg is on ioctl cal
int comms_bus_send(uint8_t dest_addr, const uint8_t *data, uint16_t length) {
    if (bus_fd < 0 || data == NULL) return -1;

    if (length > MAX_FRAME_PAYLOAD) {
        fprintf(
            stderr,
            "[COMMS BUS] send: payload length %u exceeds limit\n",
            length
        );
        return COMMS_BUS_ERROR;
    }

    uint8_t i2c_addr;

    if (lookup_i2c_addr(dest_addr, &i2c_addr) < 0) {
        fprintf(
            stderr,
            "[COMMS BUS] send: no physical I2C address for CSP address %u\n",
            dest_addr
        );
    }

    // create the actual frame
    Frame frame = {
        .dest_addr = dest_addr,
        .src_addr = my_bus_address,
        .length = length,
    };

    memcpy(frame.payload, data, length); // copy in the payload/data

    uint8_t transaction_buffer[I2C_WRITE_BUFFER_SIZE];

    /*
    Byte zero tells the STM32 how to interpret the rest of this I2C write.
    */
    transaction_buffer[0] = (uint8_t)I2C_CMD_WRITE_FRAME;

    int frame_length = frame_serialize(
        &frame,
        &transaction_buffer[1],
        MAX_FRAME_WIRE_SIZE
    );

    if (frame_length < 0) return COMMS_BUS_ERROR;

    struct i2c_msg msg = {
        .addr = i2c_addr,
        .flags = 0, // 0 = write
        .len = (uint16_t)(1 + frame_length),
        .buf = transaction_buffer,
    };

    if (i2c_transfer(&msg, 1U) != COMMS_BUS_OK) {
        fprintf(
            stderr,
            "[COMMS BUS] send to physical address 0x%02x failed: %s\n",
            i2c_addr,
            strerror(errno)
        );
        return COMMS_BUS_ERROR;
    }

    return length; // public transport reports caller payload bytes but not framing or command bytes
}

int comms_bus_receive(uint8_t *src_addr_out, uint8_t *buffer, uint16_t max_length)
{
    if (bus_fd < 0) return -1;

    while(1) {
        for (size_t offset = 0; offset < NUM_KNOWN_SLAVES; offset++) {
            size_t i = (next_slave_start + offset) % NUM_KNOWN_SLAVES;
            uint8_t i2c_addr = known_slaves[i].i2c_addr;

            uint8_t wire_buf[I2C_WRITE_BUFFER_SIZE];
            struct i2c_msg msg = {
                .addr = i2c_addr,
                .flags = I2C_M_RD, // flag for reading. as opposed to 0 for write
                .len = I2C_WRITE_BUFFER_SIZE,
                .buf = wire_buf,
            };
            struct i2c_rdwr_ioctl_data packet = { .msgs = &msg, .nmsgs = 1};

            if (ioctl(bus_fd, I2C_RDWR, &packet) < 0) {
                continue; // board not answering... skip
            }

            Frame frame;
            if (frame_deserialize(wire_buf, I2C_WRITE_BUFFER_SIZE, &frame) < 0) {
                fprintf(stderr, "[COMMS BUS] receive: malformed frame from 0x%02x\n", i2c_addr);
                continue;
            }

            if (frame.length == 0) {
                continue; // ACKed but nothing new. Basically has no data to send from this board.
            }

            if (frame.length > max_length) {
                fprintf(stderr,
                        "[COMMS BUS] receive: payload of %u bytes exceeds caller buffer of %u bytes\n",
                        frame.length, max_length);
                return -1;
            }

            next_slave_start = (i + 1) % NUM_KNOWN_SLAVES; // fainess for next call

            if (src_addr_out != NULL) {
                *src_addr_out = frame.src_addr;
            }
            memcpy(buffer, frame.payload, frame.length);
            return frame.length;
        }
        usleep(POLL_SLEEP_US); // full sweep found nothing. delay a bit.
    }
}
