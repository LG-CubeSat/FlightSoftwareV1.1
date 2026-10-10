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
#include <time.h>

/* The following may be red/error because you are not on a Linux device. */
#include <linux/i2c-dev.h> // I2C_RDWR, I2C_M_RD
#include <linux/i2c.h> // struct i2c_msg, struct i2c_rdwer_ioctl_data

#define I2C_BUS_PATH "/dev/i2c-1"
#define I2C_WRITE_BUFFER_SIZE (1U + MAX_FRAME_WIRE_SIZE) // fixed-size read block, see receive() below
#define POLL_SLEEP_NS 10000000L

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

static int bus_fd = -1;
static uint8_t my_bus_address;
static size_t next_slave_start = 0; // rotates who gets polled first, see receive function

/*
return monotonic time in milliseconds.
CLOCK_MONOTONIC measures elapsed time and is unaffed if the system clock is correct by NTP or manually checked
*/
static uint64_t monotonic_ms(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0U;
    }

    return ((uint64_t)now.tv_sec * 1000U) + ((uint64_t)now.tv_nsec / 1000000U);
}

static int i2c_transfer(
    struct i2c_msg *messages,
    uint32_t message_count
) {
    if (bus_fd < 0 || messages == NULL || message_count == 0U) {
        errno = EINVAL;
        return COMMS_BUS_ERROR;
    }

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

/*
Ask one STM32 whether it has a frame waiting.
This is the first combined transaction. Write GET_STATUS, repeated START, and read five status bytes
*/
static int get_slave_status(
    uint8_t i2c_addr,
    I2cStatus_t *status_out
)
{
    if (status_out == NULL) {
        return COMMS_BUS_ERROR;
    }

    uint8_t command = (uint8_t)I2C_CMD_GET_STATUS;
    uint8_t status_wire[I2C_STATUS_WIRE_SIZE];

    struct i2c_msg messages[2] = {
        {
            .addr = i2c_addr,
            .flags = 0, // wirte
            .len = 1U,
            .buf = &command
        },
        {
            .addr = i2c_addr,
            .flags = I2C_M_RD,
            .len = I2C_STATUS_WIRE_SIZE,
            .buf = status_wire
        }
    };

    if (i2c_transfer(messages, 2U) != COMMS_BUS_OK) {
        return COMMS_BUS_ERROR;
    }

    if (i2c_status_decode(
        status_wire,
        sizeof(status_wire),
        status_out
    ) != 0) {
        errno = EPROTO;
        return COMMS_BUS_ERROR;
    }

    return COMMS_BUS_OK;
}

/*
Read the exact number of bytes announced by the Get Status check of slave
*/
static int read_slave_frame(
    uint8_t i2c_addr,
    uint16_t announced_wire_length,
    Frame *frame_out
)
{
    if (frame_out == NULL ||
        announced_wire_length < FRAME_HEADER_SIZE ||
        announced_wire_length > MAX_FRAME_WIRE_SIZE) {
            errno = EINVAL;
            return COMMS_BUS_ERROR;
        }

    uint8_t command = (uint8_t)I2C_CMD_READ_FRAME;
    uint8_t frame_wire[MAX_FRAME_WIRE_SIZE] = {0};

    struct i2c_msg messages[2] = {
        {
            .addr = i2c_addr,
            .flags = 0,
            .len = 1U,
            .buf = &command
        },
        {
            .addr = i2c_addr,
            .flags = I2C_M_RD,
            .len = announced_wire_length,
            .buf = frame_wire
        }
    };

    if (i2c_transfer(messages, 2U) != COMMS_BUS_OK) {
        return COMMS_BUS_ERROR;
    }

    if (frame_deserialize(
        frame_wire,
        (int)announced_wire_length,
        frame_out
    ) < 0) {
        errno = EPROTO;
        return COMMS_BUS_ERROR;
    }

    // decoded frame must consumeexactly the number of bytes expected from announced
    uint16_t decoded_wire_length = (uint16_t)(FRAME_HEADER_SIZE + frame_out->length);
    if (decoded_wire_length != announced_wire_length) {
        errno = EPROTO;
        return COMMS_BUS_ERROR;
    }

    return COMMS_BUS_OK;
}

static int lookup_i2c_addr(
    uint8_t csp_addr,
    uint8_t *i2c_addr_out
)
{
    if (i2c_addr_out == NULL) {
        return -1;
    }

    for (size_t i = 0; i < NUM_KNOWN_SLAVES; ++i) {
        if (known_slaves[i].csp_addr == csp_addr) {
            *i2c_addr_out = known_slaves[i].i2c_addr;
            return 0;
        }
    }
    return -1;
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

    if (bus_fd >= 0) {
        close(bus_fd);
        bus_fd = -1;
    }

    bus_fd = open(I2C_BUS_PATH, O_RDWR | O_CLOEXEC); // read/write
    if (bus_fd < 0) {
        fprintf(stderr, "[COMMS BUS] open(%s) failed: %s\n", I2C_BUS_PATH, strerror(errno));
        return COMMS_BUS_ERROR;
    }
    
    // I2C_FUNC_I2C means the adapter supports raw I2C transactions. includirng the repeated-start behavior used by I2C_RDWR
    unsigned long functionality = 0;

    if (ioctl(bus_fd, I2C_FUNCS, &functionality) < 0) {
        fprintf(
            stderr,
            "[COMMS BUS] I2C_FUNCS failed: %s\n",
            strerror(errno)
        );
        close(bus_fd);
        bus_fd = -1;
        return COMMS_BUS_ERROR;
    }

    if ((functionality & I2C_FUNC_I2C) == 0U) {
        fprintf(
            stderr,
            "[COMMS BUS] %s does not support raw combined I2C transactions.\n",
            I2C_BUS_PATH
        );
        close(bus_fd);
        bus_fd = -1;
        return COMMS_BUS_ERROR;
    }

    my_bus_address = my_address;

    printf("[COMMS BUS] OBC I2C master ready on %s, address 0x%02x\n", I2C_BUS_PATH, my_bus_address);
    return COMMS_BUS_OK;
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

    /*
    Byte zero tells the STM32 how to interpret the rest of this I2C write.
    */
    uint8_t transaction_buffer[I2C_WRITE_BUFFER_SIZE];
    transaction_buffer[0] = (uint8_t)I2C_CMD_WRITE_FRAME;

    int serialized_length = frame_serialize(
        &frame,
        &transaction_buffer[1],
        MAX_FRAME_WIRE_SIZE
    );

    if (serialized_length < 0) return COMMS_BUS_ERROR;

    struct i2c_msg msg = {
        .addr = i2c_addr, // which chip
        .flags = 0, // 0 = write, 1 = read
        .len = (uint16_t)(1 + frame.length), // how many bytes
        .buf = transaction_buffer, // where do they come/go-to
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

    return (int)length; // public transport reports caller payload bytes but not framing or command bytes
}

int comms_bus_receive(uint8_t *src_addr_out, uint8_t *buffer, uint16_t max_length)
{
    if (bus_fd < 0 || buffer == NULL || max_length == 0U) return COMMS_BUS_ERROR;

    uint64_t start_ms = monotonic_ms();
    uint64_t deadline_ms = start_ms + (uint64_t)COMMS_BUS_RECEIVE_TIMEOUT_MS;

    int saw_valid_status = 0;

    while(monotonic_ms() < deadline_ms) {
        for (size_t offset = 0; offset < NUM_KNOWN_SLAVES; offset++) {
            size_t index = (next_slave_start + offset) % NUM_KNOWN_SLAVES;
            
            const known_slave_t *slave = &known_slaves[index];

            I2cStatus_t status;

            if (get_slave_status(
                slave->i2c_addr,
                &status
            ) != COMMS_BUS_OK) {
                // one unavailable peripehral should not prevent other checks
                continue;
            }

            saw_valid_status = 1;

            if ((status.flags & I2C_STATUS_DATA_READY) == 0U) {
                continue;
            }
            
            Frame frame;

            if (read_slave_frame(
                slave->i2c_addr,
                status.frame_length,
                &frame
            ) != COMMS_BUS_OK) {
                fprintf(
                    stderr,
                    "[COMMS BUS] failed readig frame form %s at 0x%02x: %s\n",
                    slave->name,
                    slave->i2c_addr,
                    strerror(errno)
                );
                return COMMS_BUS_ERROR;
            }

            /* 
            the physical peripehral and logical frame source must agree.
            */
            if (frame.src_addr != slave->csp_addr) {
                fprintf(
                    stderr,
                    "[COMMS BUS] %s reported logical source %u; expected %u\n",
                    slave->name,
                    frame.src_addr,
                    slave->csp_addr
                );
                return COMMS_BUS_ERROR;
            }

            if (frame.dest_addr != my_bus_address) {
                fprintf(
                    stderr,
                    "[COMMS BUS] frame for address %u arrived at address %u\n",
                    frame.dest_addr,
                    my_bus_address
                );
                return COMMS_BUS_ERROR;
            }

            if (frame.length > max_length) {
                fprintf(stderr,
                        "[COMMS BUS] receive: payload of %u bytes exceeds caller buffer of %u bytes\n",
                        frame.length, max_length);
                return COMMS_BUS_ERROR;
            }

            memcpy(buffer, frame.payload, frame.length);
            
            if (src_addr_out != NULL) {
                *src_addr_out = frame.src_addr;
            }
            
            /*
            Poll this peripheral last next time, allowing another device
            a fair chance to deliver its frame.
            */
            next_slave_start = (index + 1U) % NUM_KNOWN_SLAVES;
            
            return (int)frame.length;
        }

        struct timespec pause = {
            .tv_sec = 0,
            .tv_nsec = POLL_SLEEP_NS
        };

        while(nanosleep(&pause, &pause) != 0 && errno == EINTR) {
            // just wait for completed portion of sleep.
        }
    }
    
    /*
    At least one valid response means the bus worked but was idle.
    No valid responses suggest a bus, wiring, address, or device error.
    */

    if (saw_valid_status) {
        return COMMS_BUS_TIMEOUT;
    }

    return COMMS_BUS_ERROR;
}
