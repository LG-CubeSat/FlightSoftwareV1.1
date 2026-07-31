#include <stdio.h>

#include "system/system.h"
#include "communication/spi.h"
#include "adcs/adcs.h"
#include "v_bus.h"

int main(void)
{
    printf("Los Gatos Cubesat OBC starting...\n");

    system_initialize();

    adcs_initialize();

    v_bus_initialize(1); // 1 means master

    printf("Sending command to ADCS: Point to Sun \n");
    adcs_point_to_sun();
    
    printf("Command sent.\n");

    return 0;
}