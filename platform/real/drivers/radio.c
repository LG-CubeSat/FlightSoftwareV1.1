#include "radio.h"
#include "radio_pins.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <gpiod.h>

/*
E22-400T30D over UART, in Normal/transparent-transmission mode (M0=0, M1=0):
data written to the UART just goes out over the air, and vice versa on receive
-- no extra framing needed on our end for that mode. AUX goes LOW while the
module is mid-operation (tx/rx/mode-switch) and HIGH when it's free; we gate
every send/receive on that so we never hand the module bytes while it's busy.
*/

#define AUX_POLL_SLEEP_US 2000
#define AUX_TIMEOUT_US 2000000 // safety cap so a stuck AUX pin can't hang forever

static int uart_fd = -1;
static struct gpiod_chip *gpio_chip = NULL;
static struct gpiod_line *line_m0 = NULL;
static struct gpiod_line *line_m1 = NULL;
static struct gpiod_line *line_aux = NULL;
static int radio_initialized = 0;

static int wait_for_aux_idle(void)
{
    int waited_us = 0;
    while (gpiod_line_get_value(line_aux) == 0) {
        if (waited_us >= AUX_TIMEOUT_US) {
            fprintf(stderr, "[RADIO] AUX stayed busy past %dus timeout\n", AUX_TIMEOUT_US);
            return -1;
        }
        usleep(AUX_POLL_SLEEP_US);
        waited_us += AUX_POLL_SLEEP_US;
    }
    return 0;
}

static int radio_gpio_initialize(void)
{
    gpio_chip = gpiod_chip_open_by_name(RADIO_GPIO_CHIP);
    if (!gpio_chip) {
        fprintf(stderr, "[RADIO] gpiod_chip_open_by_name(%s) failed: %s\n", RADIO_GPIO_CHIP, strerror(errno));
        return -1;
    }

    line_m0 = gpiod_chip_get_line(gpio_chip, RADIO_GPIO_LINE_M0);
    line_m1 = gpiod_chip_get_line(gpio_chip, RADIO_GPIO_LINE_M1);
    line_aux = gpiod_chip_get_line(gpio_chip, RADIO_GPIO_LINE_AUX);
    if (!line_m0 || !line_m1 || !line_aux) {
        fprintf(stderr, "[RADIO] gpiod_chip_get_line failed for one of M0/M1/AUX\n");
        return -1;
    }

    if (gpiod_line_request_output(line_m0, "radio_m0", 0) < 0 ||
        gpiod_line_request_output(line_m1, "radio_m1", 0) < 0) {
        fprintf(stderr, "[RADIO] gpiod_line_request_output failed: %s\n", strerror(errno));
        return -1;
    }

    if (gpiod_line_request_input(line_aux, "radio_aux") < 0) {
        fprintf(stderr, "[RADIO] gpiod_line_request_input(AUX) failed: %s\n", strerror(errno));
        return -1;
    }

    // M0=0, M1=0 -- Normal/transparent mode, the mode radio_send/receive assume.
    gpiod_line_set_value(line_m0, 0);
    gpiod_line_set_value(line_m1, 0);

    usleep(2000); // let the module settle if it was previously in a different mode

    return wait_for_aux_idle();
}

static int radio_uart_initialize(void)
{
    uart_fd = open(RADIO_UART_DEVICE, O_RDWR | O_NOCTTY);
    if (uart_fd < 0) {
        fprintf(stderr, "[RADIO] open(%s) failed: %s\n", RADIO_UART_DEVICE, strerror(errno));
        return -1;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(uart_fd, &tty) != 0) {
        fprintf(stderr, "[RADIO] tcgetattr failed: %s\n", strerror(errno));
        return -1;
    }

    cfsetispeed(&tty, RADIO_UART_BAUD);
    cfsetospeed(&tty, RADIO_UART_BAUD);

    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~PARENB;  // no parity -- E22 default framing is 8N1
    tty.c_cflag &= ~CSTOPB;  // one stop bit
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;      // 8 data bits
    tty.c_cflag &= ~CRTSCTS; // no hardware flow control

    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // raw input, no line editing
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~OPOST; // raw output

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 10; // 1.0s read timeout, so radio_receive() returns instead of blocking forever

    if (tcsetattr(uart_fd, TCSANOW, &tty) != 0) {
        fprintf(stderr, "[RADIO] tcsetattr failed: %s\n", strerror(errno));
        return -1;
    }

    tcflush(uart_fd, TCIOFLUSH);
    return 0;
}

static int radio_ensure_initialized(void)
{
    if (radio_initialized) return 0;

    if (radio_gpio_initialize() < 0) return -1;
    if (radio_uart_initialize() < 0) return -1;

    radio_initialized = 1;
    printf("[RADIO] E22-400T30D ready on %s (Normal/transparent mode)\n", RADIO_UART_DEVICE);
    fflush(stdout);
    return 0;
}

int radio_send(const uint8_t *data, size_t length)
{
    if (radio_ensure_initialized() < 0) return -1;
    if (wait_for_aux_idle() < 0) return -1; // don't write while the module is still mid-operation

    ssize_t written = write(uart_fd, data, length);
    if (written < 0) {
        fprintf(stderr, "[RADIO] write failed: %s\n", strerror(errno));
        return -1;
    }
    if ((size_t)written != length) {
        fprintf(stderr, "[RADIO] short write: %zd of %zu bytes\n", written, length);
        return -1;
    }

    // AUX drops while the module is actually transmitting over the air; wait for
    // it back high so the caller knows the module finished, not just that the
    // bytes made it onto the UART.
    if (wait_for_aux_idle() < 0) return -1;

    printf("[RADIO] transmitted %zu bytes\n", length);
    fflush(stdout);
    return (int)written;
}

int radio_receive(uint8_t *buffer, uint16_t max_length)
{
    if (radio_ensure_initialized() < 0) return -1;

    ssize_t received = read(uart_fd, buffer, max_length);
    if (received < 0) {
        fprintf(stderr, "[RADIO] read failed: %s\n", strerror(errno));
        return -1;
    }

    if (received > 0) {
        printf("[RADIO] received %zd bytes\n", received);
        fflush(stdout);
    }

    return (int)received; // 0 = nothing arrived before the read timeout, not an error
}
