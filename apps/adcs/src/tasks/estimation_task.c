#include "tasks/estimation_task.h"

#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "estimation/attitude_estimator.h"
#include "estimation/reference_vectors.h"
#include "manager/adcs_manager.h"
#include "manager/fault_manager.h"
#include "simulation/adcs_simulator.h"
#include "tasks/sensor_task.h"

#define ESTIMATION_TASK_PRIORITY 3
#define ESTIMATION_TASK_STACK_SIZE 2048

static StackType_t estimation_task_stack[ESTIMATION_TASK_STACK_SIZE];
static StaticTask_t estimation_task_buffer;
static adcs_attitude_estimator_t estimator;
static atomic_uchar reset_requested;

TaskHandle_t xEstimationHandle;

static adcs_attitude_estimator_config_t estimator_config(void) {
    return (adcs_attitude_estimator_config_t) {
        .kalman = {
            .gyro_noise_rad_s_sqrt_hz = 0.0005F,
            .gyro_bias_walk_rad_s2_sqrt_hz = 0.00001F,
            .initial_attitude_variance_rad2 = 0.05F,
            .initial_bias_variance_rad2_s2 = 0.0001F,
            .minimum_measurement_variance = 1.0e-8F,
            .maximum_innovation = 1.2F
        },
        .magnetometer_measurement_variance = 0.0001F,
        .sun_sensor_measurement_variance = 0.000025F,
        .minimum_magnetic_field_t = 1.0e-6F,
        .maximum_magnetic_field_t = 1.0e-3F,
        .minimum_sun_irradiance_w_m2 = 100.0F,
        .maximum_sample_gap_s = 0.25F,
        .maximum_consecutive_rejections = 100U
    };
}

void estimation_task_request_reset(void) {
    atomic_store(&reset_requested, 1U);
}

void estimation_task_init(void) {
    adcs_attitude_estimator_config_t config = estimator_config();

    atomic_store(&reset_requested, 0U);
    adcs_attitude_estimator_init(&estimator, &config);
    xEstimationHandle = xTaskCreateStatic(
        estimation_task,
        "Estimation",
        ESTIMATION_TASK_STACK_SIZE,
        NULL,
        ESTIMATION_TASK_PRIORITY,
        estimation_task_stack,
        &estimation_task_buffer);
    if (xEstimationHandle == NULL) {
        printf("[ESTIMATION] Task creation failed.\n");
    }
}

void estimation_task(void *parameters) {
    (void)parameters;

    for (;;) {
        adcs_sensor_packet_t sensors;
        adcs_orbit_state_t orbit;
        adcs_reference_vectors_t references;
        adcs_attitude_state_t attitude;
        uint32_t notification;

        if (!sensor_task_receive(&sensors, portMAX_DELAY)) {
            adcs_manager_note_dropped_message();
            continue;
        }
        if (adcs_simulator_get_orbit(&orbit) != ADCS_RESULT_OK ||
            adcs_reference_vectors_compute(
                adcs_simulator_get_unix_time_us(),
                &orbit,
                &references) != ADCS_RESULT_OK) {
            memset(&orbit, 0, sizeof(orbit));
            memset(&references, 0, sizeof(references));
        }
        adcs_manager_set_references(&orbit, &references);

        if (atomic_exchange(&reset_requested, 0U) != 0U) {
            adcs_manager_state_t snapshot;
            versor seed = {0.0F, 0.0F, 0.0F, 1.0F};
            adcs_manager_get_state(&snapshot);
            if (snapshot.latest_attitude.valid != 0U) {
                memcpy(seed, snapshot.latest_attitude.quaternion, sizeof(seed));
            }
            (void)adcs_attitude_estimator_reset(&estimator, seed);
            adcs_manager_note_estimator_reset();
        }

        adcs_result_t estimate_result = adcs_attitude_estimator_update(
                &estimator,
                &sensors,
                &references,
                &attitude);
        if (estimate_result == ADCS_RESULT_OUT_OF_RANGE ||
            fault_management_check_estimator_bounds(&attitude) != 0) {
            fault_management_trigger_reset(RESET_REASON_OUT_OF_BOUNDS);
        }
        if (estimate_result == ADCS_RESULT_OK) {
            adcs_manager_set_attitude(&attitude);
        } else if (attitude.timestamp_us != 0U) {
            attitude.valid = 0U;
            adcs_manager_set_attitude(&attitude);
        }

        if (xTaskNotifyWait(0U, UINT32_MAX, &notification, 0U) == pdTRUE) {
            printf("[ESTIMATION] Updating attitude estimate for target position: %d\n",
                   (int32_t)notification);
            fflush(stdout);
        }
    }
}
