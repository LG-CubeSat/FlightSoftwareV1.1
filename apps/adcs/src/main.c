#include <stdio.h>
#include <string.h>

#include "../../../rtos/kernels/include/FreeRTOS.h"
#include "task.h"

#include "command_task.h"
#include "control_task.h"
#include "estimation_task.h"
#include "housekeeping_task.h"
#include "sensor_task.h"
#include "telemetry_task.h"

#include "communication/command_handler.h"

#include "csp_network.h"
#include "csp_commands.h"
#include "fault_manager.h"

int main(void)
{
    printf("\n");
    printf("_____________\n");
    printf("ADCS Initializing\n");
    printf("_____________\n");
    fflush(stdout);

    csp_network_init(ADCS_ADDRESS, /* is_master = */ 0);

    fault_management_init();

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

