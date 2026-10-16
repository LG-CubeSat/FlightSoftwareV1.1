#include "adcs_log.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#define ADCS_LOG_BUFFER_SIZE 256U
#define ADCS_LOG_UART_TIMEOUT_MS 100U
#define ADCS_LOG_UART_CHUNK_SIZE 256U

/*
The UART selected by adcs_log_init()
it remains NULL until the UART has been init. This prevents
accidental trying to transmit through an uniinitilized peripheral.
*/
static UART_HandleTypeDef *s_log_uart = NULL;

static const char *adcs_log_level_name(adcs_log_level_t level)
{
    switch (level)
    {
        case ADCS_LOG_LEVEL_DEBUG:
            return "DEBUG";
        case ADCS_LOG_LEVEL_INFO:
            return "INFO";
        case ADCS_LOG_LEVEL_WARN:
            return "WARN";
        case ADCS_LOG_LEVEL_ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

static size_t adcs_log_bounded_length(
    const char *text,
    size_t maximum_length
)
{
    size_t length = 0U;
    while ((length < maximum_length) && (text[length] != '\0')) {
        length++;
    }

    return length;
}

void adcs_log_init(UART_HandleTypeDef *uart)
{
    s_log_uart = uart;

    /*
    Disable stdio buffering
    without this, printf() may wait before handing text to _write()
    For debugging flight sw we want each message to auto leave.
    */
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
}

HAL_StatusTypeDef adcs_log_write(
    adcs_log_level_t level,
    const char *component,
    const char *format,
    ...
)
{
    char line[ADCS_LOG_BUFFER_SIZE];
    const char *safe_component;
    int prefix_result;
    size_t used;
    size_t length;
    va_list arguments;

    if ((s_log_uart == NULL) || (format == NULL))
    {
        return HAL_ERROR;
    }

    safe_component = (component != NULL) ? component : "GENERAL";

    /*
    HAL_GetTick() is the number of ms since HAL_Init().
    It is not UTC time. But it is useful for relative boot time
    */
    prefix_result = snprintf(
        line,
        sizeof(line),
        "[%010lu ms] [%-5s] [%-8s]",
        (unsigned long)HAL_GetTick(),
        adcs_log_level_name(level),
        safe_component
    );

    if (prefix_result < 0) {
        return HAL_ERROR;
    }

    if ((size_t)prefix_result >= sizeof(line))
    {
        used = sizeof(line) - 1U;
    } else {
        used = (size_t)prefix_result;
    }

    /*
    Insert the caller's formatted message after the prefix.
    vsnprintf() is bounded: it cannot write beyond the remaining room in line[].
    this matters because malformed logging cannot seep into memory and corrupt it.
    */
    va_start(arguments, format);

    (void)vsnprintf(
        &line[used],
        sizeof(line) - used,
        format,
        arguments
    );

    va_end(arguments);

    length = adcs_log_bounded_length(line, sizeof(line));

    /*
    Leave room for CR, LF, and the terminating null byte.
    "\r\n" works cleanly with common serial terminals.
    */
    if (length > (sizeof(line) - 3U))
    {
        length = sizeof(line) - 3U;
    }

    line[length++] = '\r';
    line[length++] = '\n';
    line[length] = '\0';

    return HAL_UART_Transmit(
        s_log_uart,
        (uint8_t *)line,
        (uint16_t)length,
        ADCS_LOG_UART_TIMEOUT_MS
    );
}