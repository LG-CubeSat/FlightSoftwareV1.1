#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "tasks/command_task.h"
#include "tasks/control_task.h"
#include "tasks/estimation_task.h"
#include "tasks/housekeeping_task.h"
#include "tasks/sensor_task.h"
#include "tasks/telemetry_task.h"

#include "communication/command_handler.h"

#include "csp_network.h"
#include "csp_commands.h"

int main(void)
{
    printf("\n");
    printf("_____________\n");
    printf("ADCS Initializing\n");
    printf("_____________\n");
    fflush(stdout);

    csp_network_init(ADCS_ADDRESS, /* is_master = */ 0);

    command_handler_init();

    // Initialize tasks
    command_task_init();

    control_task_init();

    estimation_task_init();

    housekeeping_task_init();

    sensor_task_init();

    telemetry_task_init();
    
    printf("[ADCS] Starting FreeRTOS scheduler...\n\n");
    fflush(stdout);

    vTaskStartScheduler();

    printf("[ADCS] ERROR: Scheduler stopped!\n");

    return 1;
}

