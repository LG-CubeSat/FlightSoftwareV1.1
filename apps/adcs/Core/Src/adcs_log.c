#include "adcs_log.h"
#include "adcs_board.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#define ADCS_LOG_BUFFER_SIZE       256U
#define ADCS_LOG_UART_TIMEOUT_MS   100U
#define ADCS_LOG_UART_CHUNK_SIZE   256U

/* The UART selected by adcs_log_init(). */
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

    while ((length < maximum_length) && (text[length] != '\0'))
    {
        length++;
    }

    return length;
}

void adcs_log_init(UART_HandleTypeDef *uart)
{
    s_log_uart = uart;

    /* Make stdout and stderr hand each printf() to _write() immediately. */
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

    prefix_result = snprintf(
        line,
        sizeof(line),
        "[%010lu ms] [%-5s] [%-8s] ",
        (unsigned long)HAL_GetTick(),
        adcs_log_level_name(level),
        safe_component
    );

    if (prefix_result < 0)
    {
        return HAL_ERROR;
    }

    if ((size_t)prefix_result >= sizeof(line))
    {
        used = sizeof(line) - 1U;
    }
    else
    {
        used = (size_t)prefix_result;
    }

    va_start(arguments, format);
    (void)vsnprintf(&line[used], sizeof(line) - used, format, arguments);
    va_end(arguments);

    length = adcs_log_bounded_length(line, sizeof(line));

    /* Reserve room for CR, LF, and the terminating null byte. */
    if (length > (sizeof(line) - 3U))
    {
        length = sizeof(line) - 3U;
    }

    line[length++] = '\r';
    line[length++] = '\n';
    line[length] = '\0';

    adcs_stdio_lock();
    HAL_StatusTypeDef status = HAL_UART_Transmit(
        s_log_uart,
        (uint8_t *)line,
        (uint16_t)length,
        ADCS_LOG_UART_TIMEOUT_MS
    );
    adcs_stdio_unlock();
    return status;
}

/*
 * Retarget printf(). CubeIDE's generated _write() is weak, so this strong
 * implementation replaces it at link time.
 */
int _write(int file, char *data, int length)
{
    int transmitted = 0;

    (void)file;

    if ((s_log_uart == NULL) || (data == NULL) || (length <= 0))
    {
        return -1;
    }

    adcs_stdio_lock();
    while (transmitted < length)
    {
        int next_nl = -1;
        for (int i = transmitted; i < length; i++)
        {
            if (data[i] == '\n')
            {
                next_nl = i;
                break;
            }
        }

        if (next_nl >= 0)
        {
            int prefix_len = next_nl - transmitted;
            if (prefix_len > 0)
            {
                HAL_UART_Transmit(
                    s_log_uart,
                    (uint8_t *)&data[transmitted],
                    (uint16_t)prefix_len,
                    ADCS_LOG_UART_TIMEOUT_MS
                );
            }
            static const uint8_t crlf[2] = {'\r', '\n'};
            if (next_nl > 0 && data[next_nl - 1] == '\r')
            {
                HAL_UART_Transmit(s_log_uart, (uint8_t *)"\n", 1, ADCS_LOG_UART_TIMEOUT_MS);
            }
            else
            {
                HAL_UART_Transmit(s_log_uart, (uint8_t *)crlf, 2, ADCS_LOG_UART_TIMEOUT_MS);
            }
            transmitted = next_nl + 1;
        }
        else
        {
            int remaining = length - transmitted;
            uint16_t chunk_length =
                (remaining > (int)ADCS_LOG_UART_CHUNK_SIZE)
                    ? (uint16_t)ADCS_LOG_UART_CHUNK_SIZE
                    : (uint16_t)remaining;

            HAL_StatusTypeDef status = HAL_UART_Transmit(
                s_log_uart,
                (uint8_t *)&data[transmitted],
                chunk_length,
                ADCS_LOG_UART_TIMEOUT_MS
            );

            if (status != HAL_OK)
            {
                adcs_stdio_unlock();
                return (transmitted > 0) ? transmitted : -1;
            }

            transmitted += (int)chunk_length;
        }
    }

    adcs_stdio_unlock();
    return transmitted;
}
