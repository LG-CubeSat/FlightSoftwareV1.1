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
#define AUX_BUSY_GRACE_US 50000 // allow up to 50ms for AUX to assert LOW after we hand the module bytes

static int uart_fd = -1;
static struct gpiod_chip *gpio_chip = NULL;
static struct gpiod_line *line_m0 = NULL;
static struct gpiod_line *line_m1 = NULL;
static struct gpiod_line *line_aux = NULL;
static int radio_initialized = 0;

static void radio_gpio_release(void)
{
    if (line_m0) { gpiod_line_release(line_m0); line_m0 = NULL; }
    if (line_m1) { gpiod_line_release(line_m1); line_m1 = NULL; }
    if (line_aux) { gpiod_line_release(line_aux); line_aux = NULL; }
    if (gpio_chip) { gpiod_chip_close(gpio_chip); gpio_chip = NULL; }
}

static void radio_uart_release(void)
{
    if (uart_fd >= 0) { close(uart_fd); uart_fd = -1; }
}

static int wait_for_aux_idle(void)
{
    int waited_us = 0;
    for (;;) {
        int val = gpiod_line_get_value(line_aux);
        if (val < 0) {
            fprintf(stderr, "[RADIO] gpiod_line_get_value(AUX) failed: %s\n", strerror(errno));
            return -1;
        }
        if (val != 0) return 0; // AUX HIGH -- module idle
        if (waited_us >= AUX_TIMEOUT_US) {
            fprintf(stderr, "[RADIO] AUX stayed busy past %dus timeout\n", AUX_TIMEOUT_US);
            return -1;
        }
        usleep(AUX_POLL_SLEEP_US);
        waited_us += AUX_POLL_SLEEP_US;
    }
}

/*
Immediately after writing to the UART, AUX may still read HIGH from *before*
the write -- the module needs a brief moment to notice the new bytes and
pull AUX low to signal it's processing/transmitting them. Checking only
wait_for_aux_idle() right after a write could return success instantly,
before the module has even started, let alone finished. So give it a short
grace window to drop AUX first, then fall through to the normal (long
timeout) wait for it to come back up.
*/
static int wait_for_aux_busy_then_idle(void)
{
    int waited_us = 0;
    for (;;) {
        int val = gpiod_line_get_value(line_aux);
        if (val < 0) {
            fprintf(stderr, "[RADIO] gpiod_line_get_value(AUX) failed: %s\n", strerror(errno));
            return -1;
        }
        if (val == 0) break; // module asserted busy
        if (waited_us >= AUX_BUSY_GRACE_US) {
            // Didn't see it drop -- plausible for a very short payload the
            // module already finished processing before we polled. Fall
            // through to wait_for_aux_idle()'s own timeout as the real
            // safety net rather than failing here.
            break;
        }
        usleep(AUX_POLL_SLEEP_US);
        waited_us += AUX_POLL_SLEEP_US;
    }
    return wait_for_aux_idle();
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
        radio_gpio_release();
        return -1;
    }

    if (gpiod_line_request_output(line_m0, "radio_m0", 0) < 0 ||
        gpiod_line_request_output(line_m1, "radio_m1", 0) < 0) {
        fprintf(stderr, "[RADIO] gpiod_line_request_output failed: %s\n", strerror(errno));
        radio_gpio_release();
        return -1;
    }

    if (gpiod_line_request_input(line_aux, "radio_aux") < 0) {
        fprintf(stderr, "[RADIO] gpiod_line_request_input(AUX) failed: %s\n", strerror(errno));
        radio_gpio_release();
        return -1;
    }

    // M0=0, M1=0 -- Normal/transparent mode, the mode radio_send/receive assume.
    if (gpiod_line_set_value(line_m0, 0) < 0 || gpiod_line_set_value(line_m1, 0) < 0) {
        fprintf(stderr, "[RADIO] gpiod_line_set_value failed for M0/M1: %s\n", strerror(errno));
        radio_gpio_release();
        return -1;
    }

    usleep(2000); // let the module settle if it was previously in a different mode

    if (wait_for_aux_idle() < 0) {
        radio_gpio_release();
        return -1;
    }
    return 0;
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
        radio_uart_release();
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
        radio_uart_release();
        return -1;
    }

    tcflush(uart_fd, TCIOFLUSH);
    return 0;
}

static int radio_ensure_initialized(void)
{
    if (radio_initialized) return 0;

    if (radio_gpio_initialize() < 0) return -1; // already released any partial GPIO state internally

    if (radio_uart_initialize() < 0) {
        radio_gpio_release(); // GPIO succeeded but UART didn't -- release it so a retry isn't blocked by "line busy"
        return -1;
    }

    radio_initialized = 1;
    printf("[RADIO] E22-400T30D ready on %s (Normal/transparent mode)\n", RADIO_UART_DEVICE);
    fflush(stdout);
    return 0;
}

int radio_send(const uint8_t *data, size_t length)
{
    if (data == NULL && length > 0) {
        fprintf(stderr, "[RADIO] radio_send: NULL data with nonzero length\n");
        return -1;
    }
    if (length == 0) return 0;

    if (radio_ensure_initialized() < 0) return -1;
    if (wait_for_aux_idle() < 0) return -1; // don't write while the module is still mid-operation

    // write() is allowed to write fewer bytes than asked for -- keep going
    // until everything's out or a real error (not just a benign signal
    // interruption) shows up.
    size_t total_written = 0;
    while (total_written < length) {
        ssize_t written = write(uart_fd, data + total_written, length - total_written);
        if (written < 0) {
            if (errno == EINTR) continue;
            fprintf(stderr, "[RADIO] write failed: %s\n", strerror(errno));
            return -1;
        }
        total_written += (size_t)written;
    }

    // write() returning only means the bytes reached the kernel's UART buffer,
    // not that they've actually gone out over the wire yet -- drain before
    // trusting AUX to reflect the module's reaction to them.
    if (tcdrain(uart_fd) != 0) {
        fprintf(stderr, "[RADIO] tcdrain failed: %s\n", strerror(errno));
        return -1;
    }

    // AUX drops while the module is actually transmitting over the air; wait for
    // it to go busy and then back high so the caller knows the module finished,
    // not just that the bytes made it onto the UART.
    if (wait_for_aux_busy_then_idle() < 0) return -1;

    printf("[RADIO] transmitted %zu bytes\n", total_written);
    fflush(stdout);
    return (int)total_written;
}

int radio_receive(uint8_t *buffer, uint16_t max_length)
{
    if (buffer == NULL && max_length > 0) {
        fprintf(stderr, "[RADIO] radio_receive: NULL buffer with nonzero max_length\n");
        return -1;
    }
    if (max_length == 0) return 0;

    if (radio_ensure_initialized() < 0) return -1;

    ssize_t received;
    do {
        received = read(uart_fd, buffer, max_length);
    } while (received < 0 && errno == EINTR);

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
