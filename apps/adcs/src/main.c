#include <stdio.h>
#include <string.h>

#include <csp/csp.h>
#include <pthread.h>
#include <netinet/in.h>

#include "FreeRTOS.h"
#include "task.h"

#include "tasks/command_task.h"
#include "tasks/control_task.h"
#include "tasks/estimation_task.h"
#include "tasks/housekeeping_task.h"
#include "tasks/sensor_task.h"
#include "tasks/telemetry_task.h"

#include "../../../shared/csp/csp_if_spi.c"

int main(void)
{
    printf("\n");
    printf("_____________\n");
    printf("ADCS Initializing\n");
    printf("_____________\n");
    fflush(stdout);

    printf("Setting up v_bus.\n");
    fflush(stdout);

    printf("[ADCS] Initializing CSP");
    csp_iface_t iface;
    csp_if_spi_conf_t if_conf;

    csp_if_spi_init(&iface, &if_conf);

    // Initialize tasks
    command_task_init();

    control_task_init();

    estimation_task_init();

    housekeeping_task_init();

    sensor_task_init();

    telemetry_task_init();
    
    printf("[ADCS] Starting FreeRTOS scheduler...\n\n");

    vTaskStartScheduler();

    printf("[ADCS] ERROR: Scheduler stopped!\n");

    return 1;
}

