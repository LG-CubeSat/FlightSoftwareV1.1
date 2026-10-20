#include "i2c_addresses.h"
#include "i2c_protocol.h"
#include "frame.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define I2C_BUS_PATH "/dev/i2c-1"

static int run_transfer(int bus_fd, struct i2c_msg *messages, uint32_t count)
{
    struct i2c_rdwr_ioctl_data transfer = {
        .msgs = messages,
        .nmsgs = count,
    };

    int result;
    do {
        result = ioctl(bus_fd, I2C_RDWR, &transfer);
    } while ((result < 0) && (errno == EINTR));

    if (result < 0)
    {
        fprintf(
            stderr,
            "I2C transfer failed: %s%s\n",
            strerror(errno),
            (errno == EIO || errno == EREMOTEIO)
                ? " (the slave did not acknowledge part of the transaction)"
                : ""
        );
        return -1;
    }

    if ((uint32_t)result != count)
    {
        fprintf(
            stderr,
            "I2C transfer incomplete: completed %d of %u messages\n",
            result,
            count
        );
        return -1;
    }

    return 0;
}

static int test_write(int bus_fd)
{
    uint8_t command = (uint8_t)I2C_CMD_GET_STATUS;
    struct i2c_msg message = {
        .addr = ADCS_I2C_ADDR,
        .flags = 0U,
        .len = 1U,
        .buf = &command,
    };

    if (run_transfer(bus_fd, &message, 1U) != 0)
    {
        return -1;
    }

    printf(
        "PASS: ADCS acknowledged write of GET_STATUS (0x%02X)\n",
        (unsigned int)command
    );
    return 0;
}

static int test_read(int bus_fd)
{
    uint8_t response = 0xFFU;
    struct i2c_msg message = {
        .addr = ADCS_I2C_ADDR,
        .flags = I2C_M_RD,
        .len = 1U,
        .buf = &response,
    };

    if (run_transfer(bus_fd, &message, 1U) != 0)
    {
        return -1;
    }

    printf(
        "PASS: raw diagnostic read returned byte 0x%02X\n",
        (unsigned int)response
    );
    return 0;
}

static int test_combined(int bus_fd)
{
    uint8_t command = (uint8_t)I2C_CMD_GET_STATUS;
    uint8_t response[I2C_STATUS_WIRE_SIZE] = {0};
    struct i2c_msg messages[2] = {
        {
            .addr = ADCS_I2C_ADDR,
            .flags = 0U,
            .len = 1U,
            .buf = &command,
        },
        {
            .addr = ADCS_I2C_ADDR,
            .flags = I2C_M_RD,
            .len = (uint16_t)sizeof(response),
            .buf = response,
        },
    };

    if (run_transfer(bus_fd, messages, 2U) != 0)
    {
        return -1;
    }

    I2cStatus_t status;
    if (i2c_status_decode(response, sizeof(response), &status) != 0)
    {
        fprintf(stderr, "FAIL: repeated-start response is malformed\n");
        return -1;
    }

    printf("PASS: repeated-start GET_STATUS returned a valid five-byte status\n");
    return 0;
}

static int test_status(int bus_fd)
{
    uint8_t command = (uint8_t)I2C_CMD_GET_STATUS;
    uint8_t wire[I2C_STATUS_WIRE_SIZE] = {0};
    struct i2c_msg messages[2] = {
        {
            .addr = ADCS_I2C_ADDR,
            .flags = 0U,
            .len = 1U,
            .buf = &command,
        },
        {
            .addr = ADCS_I2C_ADDR,
            .flags = I2C_M_RD,
            .len = I2C_STATUS_WIRE_SIZE,
            .buf = wire,
        },
    };

    if (run_transfer(bus_fd, messages, 2U) != 0)
    {
        return -1;
    }

    printf(
        "Received status bytes: %02X %02X %02X %02X %02X\n",
        (unsigned int)wire[0],
        (unsigned int)wire[1],
        (unsigned int)wire[2],
        (unsigned int)wire[3],
        (unsigned int)wire[4]
    );

    I2cStatus_t status;
    if (i2c_status_decode(wire, sizeof(wire), &status) != 0)
    {
        fprintf(stderr, "FAIL: response is not a valid ADCS status packet\n");
        return -1;
    }

    printf(
        "PASS: flags=0x%02X pending_frame_length=%u\n",
        (unsigned int)status.flags,
        (unsigned int)status.frame_length
    );
    return 0;
}

static int test_roundtrip(int bus_fd)
{
    static const uint8_t payload[] = {0x10U, 0x20U, 0x30U, 0x40U};
    Frame outbound = {
        .dest_addr = 2U,
        .src_addr = 1U,
        .length = (uint16_t)sizeof(payload),
    };
    uint8_t write_buffer[1U + MAX_FRAME_WIRE_SIZE] = {0};
    uint8_t status_command = (uint8_t)I2C_CMD_GET_STATUS;
    uint8_t status_wire[I2C_STATUS_WIRE_SIZE] = {0};
    I2cStatus_t status;
    struct timespec poll_delay = {
        .tv_sec = 0,
        .tv_nsec = 10000000L,
    };
    int frame_ready = 0;

    memcpy(outbound.payload, payload, sizeof(payload));
    write_buffer[0] = (uint8_t)I2C_CMD_WRITE_FRAME;
    int wire_length = frame_serialize(
        &outbound,
        &write_buffer[1],
        MAX_FRAME_WIRE_SIZE
    );
    if (wire_length < 0)
    {
        fprintf(stderr, "FAIL: could not serialize test frame\n");
        return -1;
    }

    struct i2c_msg write_message = {
        .addr = ADCS_I2C_ADDR,
        .flags = 0U,
        .len = (uint16_t)(1 + wire_length),
        .buf = write_buffer,
    };
    if (run_transfer(bus_fd, &write_message, 1U) != 0)
    {
        return -1;
    }

    struct i2c_msg status_messages[2] = {
        {
            .addr = ADCS_I2C_ADDR,
            .flags = 0U,
            .len = 1U,
            .buf = &status_command,
        },
        {
            .addr = ADCS_I2C_ADDR,
            .flags = I2C_M_RD,
            .len = (uint16_t)sizeof(status_wire),
            .buf = status_wire,
        },
    };

    /* Poll for up to 500 ms while the bare-metal main loop publishes the echo. */
    for (unsigned int attempt = 0U; attempt < 50U; ++attempt)
    {
        if (run_transfer(bus_fd, status_messages, 2U) != 0 ||
            i2c_status_decode(status_wire, sizeof(status_wire), &status) != 0)
        {
            fprintf(stderr, "FAIL: could not read valid status after WRITE_FRAME\n");
            return -1;
        }

        if ((status.flags & I2C_STATUS_DATA_READY) != 0U)
        {
            frame_ready = 1;
            break;
        }

        (void)nanosleep(&poll_delay, NULL);
    }

    if (frame_ready == 0 || status.frame_length != (uint16_t)wire_length)
    {
        fprintf(
            stderr,
            "FAIL: status announced flags=0x%02X length=%u, expected length=%d\n",
            (unsigned int)status.flags,
            (unsigned int)status.frame_length,
            wire_length
        );
        return -1;
    }

    uint8_t read_command = (uint8_t)I2C_CMD_READ_FRAME;
    uint8_t returned_wire[MAX_FRAME_WIRE_SIZE] = {0};
    struct i2c_msg read_messages[2] = {
        {
            .addr = ADCS_I2C_ADDR,
            .flags = 0U,
            .len = 1U,
            .buf = &read_command,
        },
        {
            .addr = ADCS_I2C_ADDR,
            .flags = I2C_M_RD,
            .len = status.frame_length,
            .buf = returned_wire,
        },
    };
    if (run_transfer(bus_fd, read_messages, 2U) != 0)
    {
        return -1;
    }

    Frame returned;
    if (frame_deserialize(returned_wire, status.frame_length, &returned) < 0 ||
        returned.dest_addr != outbound.src_addr ||
        returned.src_addr != outbound.dest_addr ||
        returned.length != outbound.length ||
        memcmp(returned.payload, outbound.payload, outbound.length) != 0)
    {
        fprintf(stderr, "FAIL: returned frame did not match the expected echo\n");
        return -1;
    }

    printf(
        "PASS: WRITE_FRAME -> GET_STATUS -> READ_FRAME round-trip (%d wire bytes)\n",
        wire_length
    );
    return 0;
}

static void print_usage(const char *program)
{
    fprintf(
        stderr,
        "Usage: %s write|read|combined|status|roundtrip\n"
        "  write     Send one-byte GET_STATUS and expect an ACK\n"
        "  read      Raw one-byte read (diagnostic invalid-command test)\n"
        "  combined  Validate repeated-start five-byte GET_STATUS\n"
        "  status    Decode and print the five-byte status\n"
        "  roundtrip Exercise WRITE_FRAME, status polling, and READ_FRAME\n",
        program
    );
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        print_usage(argv[0]);
        return 2;
    }

    int bus_fd = open(I2C_BUS_PATH, O_RDWR | O_CLOEXEC);
    if (bus_fd < 0)
    {
        fprintf(stderr, "Could not open %s: %s\n", I2C_BUS_PATH, strerror(errno));
        return 1;
    }

    unsigned long functionality = 0U;
    if (ioctl(bus_fd, I2C_FUNCS, &functionality) < 0)
    {
        fprintf(stderr, "I2C_FUNCS failed: %s\n", strerror(errno));
        close(bus_fd);
        return 1;
    }

    if ((functionality & I2C_FUNC_I2C) == 0U)
    {
        fprintf(stderr, "%s does not support raw I2C transfers\n", I2C_BUS_PATH);
        close(bus_fd);
        return 1;
    }

    printf(
        "Testing ADCS at 7-bit address 0x%02X on %s\n",
        (unsigned int)ADCS_I2C_ADDR,
        I2C_BUS_PATH
    );

    int result;
    if (strcmp(argv[1], "write") == 0)
    {
        result = test_write(bus_fd);
    }
    else if (strcmp(argv[1], "read") == 0)
    {
        result = test_read(bus_fd);
    }
    else if (strcmp(argv[1], "combined") == 0)
    {
        result = test_combined(bus_fd);
    }
    else if (strcmp(argv[1], "status") == 0)
    {
        result = test_status(bus_fd);
    }
    else if (strcmp(argv[1], "roundtrip") == 0)
    {
        result = test_roundtrip(bus_fd);
    }
    else
    {
        print_usage(argv[0]);
        result = -1;
    }

    close(bus_fd);
    return (result == 0) ? 0 : 1;
}
