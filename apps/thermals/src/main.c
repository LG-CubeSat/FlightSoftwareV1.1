#include "stdint.h"
#include "stdio.h"

#include "tasks/sensor_read_task.h"
#include "tasks/thermal_write_task.h"
#include "tasks/command_task.h"
#include "tasks/housekeeping_task.h"
#include "tasks/telemetry_task.h"
#include "communication/command_handler.h"

#include "thermal_data.h"

#include "csp_network.h"
#include "csp_commands.h"

int main(void) {



    printf("\n");
    printf("_____________\n");
    printf("Thermals Initializing\n");
    printf("_____________\n");
    fflush(stdout);

    csp_network_init(THERMALS, 0);

    command_task_init();
    command_handler_init();

    sensor_read_task_init();
    thermal_write_task_init();
    housekeeping_task_init();
    telemetry_task_init();

    printf("[Thermals] Starting FreeRTOS scheduler...\n\n");
    fflush(stdout);

    vTaskStartScheduler();

    printf("[Thermals] ERROR: Scheduler stopped!\n");


    return 0;
}
