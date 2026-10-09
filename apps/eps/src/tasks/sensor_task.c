#include "tasks/sensor_task.h"

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "battery.h"
#include "manager/eps_manager.h"
#include "power_rail.h"
#include "simulation/eps_simulator.h"
#include "solar_array.h"

#define SENSOR_TASK_PRIORITY 4
#define SENSOR_TASK_STACK_SIZE 1536
#define SENSOR_TASK_PERIOD_MS 100
#define SENSOR_QUEUE_LENGTH 1

static StackType_t sensor_task_stack[SENSOR_TASK_STACK_SIZE];
static StaticTask_t sensor_task_buffer;
static StaticQueue_t sensor_queue_buffer;
static uint8_t sensor_queue_storage[SENSOR_QUEUE_LENGTH * sizeof(eps_sensor_packet_t)];
static QueueHandle_t sensor_queue;
static uint32_t sensor_sequence;

TaskHandle_t xSensorHandle;

int sensor_task_receive(eps_sensor_packet_t *packet, TickType_t wait_ticks) {
    if (sensor_queue == NULL || packet == NULL) {
        return 0;
    }
    return xQueueReceive(sensor_queue, packet, wait_ticks) == pdPASS;
}

void sensor_task_init(void) {
    sensor_sequence = 0U;
    sensor_queue = xQueueCreateStatic(
        SENSOR_QUEUE_LENGTH,
        sizeof(eps_sensor_packet_t),
        sensor_queue_storage,
        &sensor_queue_buffer);
    if (sensor_queue == NULL) {
        printf("[SENSOR] Queue creation failed.\n");
        return;
    }

    xSensorHandle = xTaskCreateStatic(
        sensor_task,
        "Sensor",
        SENSOR_TASK_STACK_SIZE,
        NULL,
        SENSOR_TASK_PRIORITY,
        sensor_task_stack,
        &sensor_task_buffer);
    if (xSensorHandle == NULL) {
        printf("[SENSOR] Task creation failed.\n");
    }
}

void sensor_task(void *parameters) {
    TickType_t last_wake_time = xTaskGetTickCount();

    (void)parameters;
    for (;;) {
        eps_sensor_packet_t packet;
        battery_sample_t battery;
        solar_array_sample_t solar;
        power_rail_sample_t rails;
        uint32_t notification;

        memset(&packet, 0, sizeof(packet));
        eps_simulator_step((float)SENSOR_TASK_PERIOD_MS / 1000.0F);
        packet.sequence = ++sensor_sequence;
        packet.timestamp_us = eps_simulator_get_time_us();

        if (battery_read(&battery) == BATTERY_OK) {
            packet.timestamp_us = battery.timestamp_us;
            packet.pack_voltage_v = battery.pack_voltage_v;
            packet.pack_current_a = battery.pack_current_a;
            packet.battery_temperature_c = battery.temperature_c;
            packet.state_of_charge = battery.state_of_charge;
            packet.valid_mask |= EPS_SENSOR_VALID_BATTERY;
        }
        if (solar_array_read(&solar) == SOLAR_ARRAY_OK) {
            packet.solar_voltage_v = solar.voltage_v;
            packet.solar_current_a = solar.current_a;
            packet.solar_irradiance_w_m2 = solar.irradiance_w_m2;
            packet.valid_mask |= EPS_SENSOR_VALID_SOLAR;
        }
        if (power_rail_read(&rails) == POWER_RAIL_OK) {
            memcpy(
                packet.rail_voltage_v,
                rails.voltage_v,
                sizeof(packet.rail_voltage_v));
            memcpy(
                packet.rail_current_a,
                rails.current_a,
                sizeof(packet.rail_current_a));
            memcpy(
                packet.rail_enabled,
                rails.enabled,
                sizeof(packet.rail_enabled));
            packet.valid_mask |= EPS_SENSOR_VALID_RAILS;
        }

        eps_manager_set_sensors(&packet);
        if (uxQueueMessagesWaiting(sensor_queue) != 0U) {
            eps_manager_note_dropped_message();
        }
        (void)xQueueOverwrite(sensor_queue, &packet);

        if (xTaskNotifyWait(0U, UINT32_MAX, &notification, 0U) == pdTRUE) {
            printf("[SENSOR] Sampling EPS for request: %u\n", notification);
            fflush(stdout);
        }
        xTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(SENSOR_TASK_PERIOD_MS));
    }
}
