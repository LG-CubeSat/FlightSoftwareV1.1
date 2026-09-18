#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "communication/command_handler.h"
#include "communication/telemetry.h"
#include "csp_commands.h"
#include "csp_network.h"
#include "imu.h"
#include "magnetometer.h"
#include "magnetorquer.h"
#include "manager/adcs_manager.h"
#include "manager/fault_manager.h"
#include "simulation/adcs_simulator.h"
#include "sun_sensor.h"
#include "tasks/command_task.h"
#include "tasks/control_task.h"
#include "tasks/estimation_task.h"
#include "tasks/housekeeping_task.h"
#include "tasks/sensor_task.h"
#include "tasks/telemetry_task.h"

int main(int argument_count, char **arguments) {
    uint8_t standalone = 0U;

    if ((argument_count == 2 && strcmp(arguments[1], "--standalone") == 0) ||
        getenv("ADCS_STANDALONE") != NULL) {
        standalone = 1U;
    }
    if (standalone != 0U) {
        /* The simulation reset driver re-execs this process. Preserve the
           no-CSP mode across that re-exec so a watchdog reset stays safe. */
        (void)setenv("ADCS_STANDALONE", "1", 1);
    }

    printf("\n_____________\n");
    printf("ADCS Initializing\n");
    printf("_____________\n");
    fflush(stdout);

    adcs_manager_init();
    if (adcs_simulator_init(NULL) != ADCS_RESULT_OK) {
        fprintf(stderr, "[ADCS] Simulator initialization failed.\n");
        return 1;
    }
    if (imu_initialize() != IMU_OK ||
        magnetometer_initialize() != MAGNETOMETER_OK ||
        sun_sensor_initialize() != SUN_SENSOR_OK ||
        magnetorquer_initialize() != MAGNETORQUER_OK) {
        fprintf(stderr, "[ADCS] Sensor/actuator driver initialization failed.\n");
        return 1;
    }
    if (standalone == 0U) {
        csp_network_init(ADCS_ADDRESS, 0);
    }
    fault_management_set_transport_enabled(standalone == 0U);
    fault_management_init();

    command_task_init();
    sensor_task_init();
    estimation_task_init();
    control_task_init();
    telemetry_task_init();
    housekeeping_task_init();
    adcs_telemetry_set_transport_enabled(standalone == 0U);
    if (standalone == 0U) {
        command_handler_init();
    }

    printf("[ADCS] Starting FreeRTOS scheduler...\n\n");
    fflush(stdout);
    vTaskStartScheduler();

    fprintf(stderr, "[ADCS] ERROR: Scheduler stopped.\n");
    return 1;
}
