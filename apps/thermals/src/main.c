#include "stdint.h"
#include "stdio.h"


#include "FreeRTOS.h"
#include "task.h"

#include "tasks/sensor_read_task.h"
#include "tasks/thermal_write_task.h"

int main(void) {

    printf("\n");
    printf("_____________\n");
    printf("Thermals Initializing\n");
    printf("_____________\n");
    fflush(stdout);

    sensor_read_task_init();
    thermal_write_task_init();


    printf("[Thermals] Starting FreeRTOS scheduler...\n\n");
    fflush(stdout);

    vTaskStartScheduler();

    printf("[Thermals] ERROR: Scheduler stopped!\n");


    return 0;
}
