#include "stdint.h"
#include "stdio.h"


#include "FreeRTOS.h"
#include "task.h"

#include "../tasks/sensor_read_task.c"
#include "../tasks/thermal_write_task.c"


//ALL IN CELSIUS
float temperature1; //global temperature variable from sensor 1
float temperature2; //global temperature variable from sensor 2

float goal_temp1; //what the current temp of sensor1 should be
float goal_temp2; //what the current temp of sensor2 should be

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