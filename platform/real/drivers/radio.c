#include "radio.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/gpio.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/*
real e22-900t30d over uart in normal (transparent) mode.
m0 + m1 low = normal mode. aux high = idle, aux low = busy.
gpio uses the linux gpio chardev directly, no libgpiod needed.
pins from the obc schematic (pi zero 2 w).
*/

#define RADIO_UART_DEVICE "/dev/serial0" // gpio14 txd0 / gpio15 rxd0
#define RADIO_UART_BAUD B9600 // e22 factory default
#define RADIO_GPIO_CHIP "/dev/gpiochip0"
#define RADIO_PIN_M0 24
#define RADIO_PIN_M1 23
#define RADIO_PIN_AUX 25

#define AUX_IDLE_TIMEOUT_MS 5000 // slow air rates take a while to send 240 bytes
#define AUX_BUSY_WAIT_MS 50      // time for aux to drop after we hand it bytes
#define RX_TIMEOUT_MS 1000
#define RX_GAP_MS 20 // uart quiet this long = end of packet

static pthread_once_t init_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t tx_lock = PTHREAD_MUTEX_INITIALIZER; // keeps two senders' packets from interleaving
static pthread_mutex_t rx_lock = PTHREAD_MUTEX_INITIALIZER;
static int uart_fd = -1;
static int mode_fd = -1; // m0 + m1
static int aux_fd = -1;
static int ready = 0;

static long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static void sleep_ms(int ms)
{
    struct timespec ts = { .tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

static int open_uart(void)
{
    uart_fd = open(RADIO_UART_DEVICE, O_RDWR | O_NOCTTY);
    if (uart_fd < 0) {
        perror("[RADIO] open uart");
        return -1;
    }

    struct termios tty;
    if (tcgetattr(uart_fd, &tty) != 0) {
        perror("[RADIO] tcgetattr");
        return -1;
    }

    cfmakeraw(&tty); // 8 data bits, no parity, raw bytes
    tty.c_cflag &= ~(CSTOPB | CRTSCTS); // 1 stop bit, no flow control
    tty.c_cflag |= CLOCAL | CREAD;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;
    cfsetispeed(&tty, RADIO_UART_BAUD);
    cfsetospeed(&tty, RADIO_UART_BAUD);

    if (tcsetattr(uart_fd, TCSANOW, &tty) != 0) {
        perror("[RADIO] tcsetattr");
        return -1;
    }
    tcflush(uart_fd, TCIOFLUSH);
    return 0;
}

static int request_lines(int chip_fd, const unsigned int *pins, unsigned int count, uint64_t flags)
{
    struct gpio_v2_line_request req;
    memset(&req, 0, sizeof(req));

    for (unsigned int i = 0; i < count; i++) req.offsets[i] = pins[i];
    req.num_lines = count;
    req.config.flags = flags;
    strncpy(req.consumer, "e22_radio", sizeof(req.consumer) - 1);

    if (ioctl(chip_fd, GPIO_V2_GET_LINE_IOCTL, &req) < 0) {
        perror("[RADIO] gpio line request");
        return -1;
    }
    return req.fd;
}

static int read_aux(void)
{
    struct gpio_v2_line_values values = { .bits = 0, .mask = 1 };
    if (ioctl(aux_fd, GPIO_V2_LINE_GET_VALUES_IOCTL, &values) < 0) return -1;
    return (int)(values.bits & 1);
}

static int wait_for_aux(int level, int timeout_ms)
{
    long deadline = now_ms() + timeout_ms;
    for (;;) {
        int aux = read_aux();
        if (aux < 0) return -1;
        if (aux == level) return 0;
        if (now_ms() >= deadline) return -1;
        sleep_ms(1);
    }
}

static int open_gpio(void)
{
    int chip_fd = open(RADIO_GPIO_CHIP, O_RDWR | O_CLOEXEC);
    if (chip_fd < 0) {
        perror("[RADIO] open gpio chip");
        return -1;
    }

    const unsigned int mode_pins[] = { RADIO_PIN_M0, RADIO_PIN_M1 };
    const unsigned int aux_pins[] = { RADIO_PIN_AUX };
    mode_fd = request_lines(chip_fd, mode_pins, 2, GPIO_V2_LINE_FLAG_OUTPUT);
    aux_fd = request_lines(chip_fd, aux_pins, 1, GPIO_V2_LINE_FLAG_INPUT);
    close(chip_fd); // line fds stay valid without the chip fd

    if (mode_fd < 0 || aux_fd < 0) return -1;

    struct gpio_v2_line_values normal_mode = { .bits = 0, .mask = 0x3 }; // m0 = 0, m1 = 0
    if (ioctl(mode_fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &normal_mode) < 0) {
        perror("[RADIO] set normal mode");
        return -1;
    }
    return 0;
}

static void radio_init(void)
{
    if (open_gpio() != 0 || open_uart() != 0) return;

    if (wait_for_aux(1, AUX_IDLE_TIMEOUT_MS) != 0) { // module ready after mode switch
        fprintf(stderr, "[RADIO] e22 never went idle after init\n");
        return;
    }
    ready = 1;
}

static int write_all(const uint8_t *data, size_t length)
{
    size_t written = 0;
    while (written < length) {
        ssize_t n = write(uart_fd, data + written, length - written);
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN) continue;
            perror("[RADIO] uart write");
            return -1;
        }
        written += (size_t)n;
    }
    return tcdrain(uart_fd); // wait until the pi's uart has actually shifted every byte out
}

int radio_send(const uint8_t *data, size_t length)
{
    if (data == NULL || length == 0) return -1;

    pthread_once(&init_once, radio_init);
    if (!ready) return -1;

    int result = 0;
    pthread_mutex_lock(&tx_lock);

    for (size_t offset = 0; offset < length; offset += RADIO_MAX_PACKET_SIZE) {
        size_t chunk = length - offset;
        if (chunk > RADIO_MAX_PACKET_SIZE) chunk = RADIO_MAX_PACKET_SIZE;

        if (wait_for_aux(1, AUX_IDLE_TIMEOUT_MS) != 0 || write_all(data + offset, chunk) != 0) {
            result = -1;
            break;
        }

        /* aux drops while the e22 transmits. waiting for it to come back
           up keeps each chunk its own packet instead of merging with the next */
        (void)wait_for_aux(0, AUX_BUSY_WAIT_MS);
        if (wait_for_aux(1, AUX_IDLE_TIMEOUT_MS) != 0) {
            fprintf(stderr, "[RADIO] aux stuck busy after send\n");
            result = -1;
            break;
        }
    }

    pthread_mutex_unlock(&tx_lock);
    return result;
}

int radio_receive(uint8_t *buffer, uint16_t max_length)
{
    if (buffer == NULL || max_length == 0) return -1;

    pthread_once(&init_once, radio_init);
    if (!ready) return -1;

    pthread_mutex_lock(&rx_lock);

    uint8_t packet[RADIO_MAX_PACKET_SIZE];
    size_t total = 0;
    int wait_ms = RX_TIMEOUT_MS;

    /* the e22 hands us a received packet as a burst of uart bytes.
       keep reading until the line goes quiet or we have a full packet */
    while (total < sizeof(packet)) {
        struct pollfd pfd = { .fd = uart_fd, .events = POLLIN };
        int ready_fds = poll(&pfd, 1, wait_ms);
        if (ready_fds < 0) {
            if (errno == EINTR) continue;
            pthread_mutex_unlock(&rx_lock);
            return -1;
        }
        if (ready_fds == 0) break; // quiet

        ssize_t n = read(uart_fd, packet + total, sizeof(packet) - total);
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN) continue;
            pthread_mutex_unlock(&rx_lock);
            return -1;
        }
        total += (size_t)n;
        wait_ms = RX_GAP_MS;
    }

    pthread_mutex_unlock(&rx_lock);

    if (total == 0) return 0; // timed out, nothing heard
    if (total > max_length) return -1;

    memcpy(buffer, packet, total);
    return (int)total;
}
