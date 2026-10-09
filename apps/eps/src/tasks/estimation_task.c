#include "tasks/estimation_task.h"

#include <math.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "manager/eps_manager.h"
#include "tasks/sensor_task.h"
#include "utils/power_calculations.h"

#define ESTIMATION_TASK_PRIORITY 3
#define ESTIMATION_TASK_STACK_SIZE 1536
#define ESTIMATION_DEFAULT_EFFICIENCY 0.95F

static StackType_t estimation_task_stack[ESTIMATION_TASK_STACK_SIZE];
static StaticTask_t estimation_task_buffer;
static eps_energy_accumulator_t net_accumulator;
static eps_energy_accumulator_t charge_accumulator;
static eps_energy_accumulator_t discharge_accumulator;

TaskHandle_t xEstimationHandle;

void estimation_task_init(void) {
    eps_energy_accumulator_init(&net_accumulator);
    eps_energy_accumulator_init(&charge_accumulator);
    eps_energy_accumulator_init(&discharge_accumulator);
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
        eps_sensor_packet_t sensors;
        eps_power_state_t power;
        float instant_power_w;
        uint32_t notification;

        if (!sensor_task_receive(&sensors, portMAX_DELAY)) {
            eps_manager_note_dropped_message();
            continue;
        }

        /* Pack current is positive while discharging. */
        instant_power_w =
            eps_instant_power_w(sensors.pack_voltage_v, sensors.pack_current_a);

        eps_energy_accumulator_update(
            &net_accumulator,
            instant_power_w,
            sensors.timestamp_us);
        if (instant_power_w < 0.0F) {
            eps_energy_accumulator_update(
                &charge_accumulator,
                -instant_power_w,
                sensors.timestamp_us);
        } else {
            eps_energy_accumulator_update(
                &discharge_accumulator,
                instant_power_w,
                sensors.timestamp_us);
        }

        power.timestamp_us = sensors.timestamp_us;
        power.instant_power_w = instant_power_w;
        power.average_power_w =
            eps_energy_accumulator_average_power_w(&net_accumulator);
        power.energy_wh = eps_energy_accumulator_energy_wh(&net_accumulator);
        power.battery_efficiency = eps_battery_efficiency(
            eps_energy_accumulator_energy_wh(&discharge_accumulator),
            eps_energy_accumulator_energy_wh(&charge_accumulator));
        if (!isfinite(power.battery_efficiency) ||
            power.battery_efficiency <= 0.0F) {
            power.battery_efficiency = ESTIMATION_DEFAULT_EFFICIENCY;
        }
        power.power_positive = instant_power_w < 0.0F ? 1U : 0U;
        eps_manager_set_power(&power);

        if (xTaskNotifyWait(0U, UINT32_MAX, &notification, 0U) == pdTRUE) {
            printf("[ESTIMATION] Updating power estimate for request: %u\n",
                   notification);
            fflush(stdout);
        }
    }
}
