#include "adcs_board.h"
#include "adcs_log.h"
#include "FreeRTOS.h"
#include "task.h"
#include "communication/command_handler.h"
#include "communication/telemetry.h"
#include "manager/adcs_manager.h"
#include "manager/fault_manager.h"
#include "tasks/command_task.h"
#include "tasks/sensor_task.h"
#include "tasks/estimation_task.h"
#include "tasks/control_task.h"
#include "tasks/telemetry_task.h"
#include "tasks/housekeeping_task.h"
#include "imu.h"
#include "magnetometer.h"
#include "sun_sensor.h"
#include "magnetorquer.h"

void adcs_application_start(void) {
    adcs_manager_init();
    adcs_manager_set_actuators_inhibited(1U);
    int imu_ready = imu_initialize() == IMU_OK;
    int mag_ready = magnetometer_initialize() == MAGNETOMETER_OK;
    int sun_ready = sun_sensor_initialize() == SUN_SENSOR_OK;
    int rods_ready = magnetorquer_initialize() == MAGNETORQUER_OK;
    adcs_log_write(ADCS_LOG_LEVEL_INFO, "ADCS", "Drivers ready: imu=%d mag=%d sun=%d rods=%d",
                   imu_ready, mag_ready, sun_ready, rods_ready);
    fault_management_init();
    adcs_csp_init();
    command_task_init();
    sensor_task_init();
    estimation_task_init();
    control_task_init();
    telemetry_task_init();
    housekeeping_task_init();
    configASSERT(xCommandHandle && xSensorHandle && xEstimationHandle &&
                 xControlHandle && xTelemetryHandle && xHousekeepingHandle);
    adcs_telemetry_set_transport_enabled(1U);
    fault_management_set_transport_enabled(1U);
    command_handler_init();
    adcs_log_write(ADCS_LOG_LEVEL_INFO, "ADCS", "Starting FreeRTOS; actuators inhibited");
    adcs_watchdog_start();
    vTaskStartScheduler();
    adcs_assert_failed(__FILE__, __LINE__);
}
