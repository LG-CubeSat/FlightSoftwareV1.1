#ifndef ADCS_LOG_H
#define ADCS_LOG_H

#include "stm32g4xx_hal.h"

/*
 * Severity describes how important a message is.
 *
 * DEBUG: detailed development information
 * INFO:  normal operation
 * WARN:  something unexpected happened, but operation can continue
 * ERROR: an operation failed
 */
typedef enum
{
    ADCS_LOG_LEVEL_DEBUG = 0,
    ADCS_LOG_LEVEL_INFO,
    ADCS_LOG_LEVEL_WARN,
    ADCS_LOG_LEVEL_ERROR
} adcs_log_level_t;

/* Connect the logger to an initialized Cube/HAL UART. */
void adcs_log_init(UART_HandleTypeDef *uart);

/* Write one timestamped, formatted log line. */
HAL_StatusTypeDef adcs_log_write(
    adcs_log_level_t level,
    const char *component,
    const char *format,
    ...
);

#endif
