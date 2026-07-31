#ifndef SPI_TASK_H
#define SPI_TASK_H

#include "FreeRTOS.h"
#include "task.h"

void spi_task_loop(void *pvParameters);

void spi_task_run(void);

TaskHandle_t spi_task_create_static(void);

#endif