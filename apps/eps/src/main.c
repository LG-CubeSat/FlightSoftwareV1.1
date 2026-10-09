#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "battery.h"
#include "communication/command_handler.h"
#include "communication/telemetry.h"
#include "csp_commands.h"
#include "csp_network.h"
#include "manager/eps_manager.h"
#include "manager/fault_manager.h"
#include "power_rail.h"
#include "simulation/eps_simulator.h"
#include "solar_array.h"
#include "tasks/command_task.h"
#include "tasks/estimation_task.h"
#include "tasks/housekeeping_task.h"
#include "tasks/sensor_task.h"
#include "tasks/telemetry_task.h"

int main(int argument_count, char **arguments) {
    uint8_t standalone = 0U;

    if ((argument_count == 2 && strcmp(arguments[1], "--standalone") == 0) ||
        getenv("EPS_STANDALONE") != NULL) {
        standalone = 1U;
    }
    if (standalone != 0U) {
        /* The simulation reset driver re-execs this process. Preserve the
           no-CSP mode across that re-exec so a watchdog reset stays safe. */
        (void)setenv("EPS_STANDALONE", "1", 1);
    }

    printf("\n_____________\n");
    printf("EPS Initializing\n");
    printf("_____________\n");
    fflush(stdout);

    eps_manager_init();
    if (eps_simulator_init(NULL) != EPS_RESULT_OK) {
        fprintf(stderr, "[EPS] Simulator initialization failed.\n");
        return 1;
    }
    if (battery_initialize() != BATTERY_OK ||
        solar_array_initialize() != SOLAR_ARRAY_OK ||
        power_rail_initialize() != POWER_RAIL_OK) {
        fprintf(stderr, "[EPS] Sensor/actuator driver initialization failed.\n");
        return 1;
    }
    if (standalone == 0U) {
        csp_network_init(EPS_ADDRESS, 0);
    }
    fault_management_set_transport_enabled(standalone == 0U);
    fault_management_init();

    command_task_init();
    sensor_task_init();
    estimation_task_init();
    telemetry_task_init();
    housekeeping_task_init();
    eps_telemetry_set_transport_enabled(standalone == 0U);
    if (standalone == 0U) {
        command_handler_init();
    }

    printf("[EPS] Starting FreeRTOS scheduler...\n\n");
    fflush(stdout);

    vTaskStartScheduler();

    fprintf(stderr, "[EPS] ERROR: Scheduler stopped.\n");
    return 1;
}
