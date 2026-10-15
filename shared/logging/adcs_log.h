#ifndef ADCS_LOG_H
#define ADCS_LOG_H

#include "stm32g4xx_hal.h"

/*
Severity describes how important a message is.

DEBUG: detailed development information
INFO: normal operation
WARN: something unexpected happened, but operation can continue
ERROR: an operation failed
*/
typedef enum
{
    ADCS_LOG_LEVEL_DEBUG = 0,
    ADCS_LOG_LEVEL_INFO,
    ADCS_LOG_LEVEL_WARN,
    ADCS_LOG_LEVEL_ERROR
} adcs_log_level_t;

// connect the logging system to a Cube/HAL UART. This must be called after MX_USART_1_UART_Init().
void adcs_log_init(UART_HandleTypeDef *uart);

/*
Write one formatted log line.
Example:
adcs_log_write(
    ADCS_LOG_LEVEL_INFO,
    "BOOT",
    "Firmware started, reset count=%lu",
    reset_count
);
*/
HAL_StatusTypeDef adcs_log_write( 
    adcs_log_level_t level,
    const char *component,
    const char *format,
    ...
);

#endif