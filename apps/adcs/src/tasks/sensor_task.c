#include "tasks/sensor_task.h"

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "manager/adcs_manager.h"
#include "simulation/adcs_simulator.h"

#define SENSOR_TASK_PRIORITY 4
#define SENSOR_TASK_STACK_SIZE 1536
#define SENSOR_TASK_PERIOD_MS 10
#define SENSOR_QUEUE_LENGTH 1

static StackType_t sensor_task_stack[SENSOR_TASK_STACK_SIZE];
static StaticTask_t sensor_task_buffer;
static StaticQueue_t sensor_queue_buffer;
static uint8_t sensor_queue_storage[SENSOR_QUEUE_LENGTH * sizeof(adcs_sensor_packet_t)];
static QueueHandle_t sensor_queue;
static uint32_t sensor_sequence;

TaskHandle_t xSensorHandle;

int sensor_task_receive(adcs_sensor_packet_t *packet, TickType_t wait_ticks) {
    if (sensor_queue == NULL || packet == NULL) {
        return 0;
    }
    return xQueueReceive(sensor_queue, packet, wait_ticks) == pdPASS;
}

void sensor_task_init(void) {
    sensor_sequence = 0U;
    sensor_queue = xQueueCreateStatic(
        SENSOR_QUEUE_LENGTH,
        sizeof(adcs_sensor_packet_t),
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
        adcs_sensor_packet_t packet;
        uint64_t sample_timestamp;
        uint32_t notification;

        memset(&packet, 0, sizeof(packet));
        (void)adcs_simulator_step((float)SENSOR_TASK_PERIOD_MS / 1000.0F);
        packet.sequence = ++sensor_sequence;
        packet.timestamp_us = adcs_simulator_get_time_us();

        if (adcs_simulator_read_imu(
                &sample_timestamp,
                packet.angular_rate_rad_s,
                packet.acceleration_m_s2) == ADCS_RESULT_OK) {
            packet.valid_mask |= ADCS_SENSOR_VALID_GYROSCOPE |
                                 ADCS_SENSOR_VALID_ACCELEROMETER;
        }
        if (adcs_simulator_read_magnetometer(
                &sample_timestamp,
                packet.magnetic_field_t) == ADCS_RESULT_OK) {
            packet.valid_mask |= ADCS_SENSOR_VALID_MAGNETOMETER;
        }
        if (adcs_simulator_read_sun_sensor(
                &sample_timestamp,
                packet.sun_vector_body,
                &packet.sun_irradiance_w_m2) == ADCS_RESULT_OK) {
            packet.valid_mask |= ADCS_SENSOR_VALID_SUN;
        }
        if (adcs_simulator_read_thermistor(
                &sample_timestamp,
                &packet.board_temperature_c) == ADCS_RESULT_OK) {
            packet.valid_mask |= ADCS_SENSOR_VALID_TEMPERATURE;
        }

        adcs_manager_set_sensors(&packet);
        if (xQueueOverwrite(sensor_queue, &packet) != pdPASS) {
            adcs_manager_note_dropped_message();
        }

        if (xTaskNotifyWait(0U, UINT32_MAX, &notification, 0U) == pdTRUE) {
            printf("[SENSOR] Sampling sensors for move to position: %d\n",
                   (int32_t)notification);
            fflush(stdout);
        }
        xTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(SENSOR_TASK_PERIOD_MS));
    }
}
