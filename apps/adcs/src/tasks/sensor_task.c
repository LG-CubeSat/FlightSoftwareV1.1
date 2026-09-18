#include "tasks/sensor_task.h"

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "imu.h"
#include "magnetometer.h"
#include "manager/adcs_manager.h"
#include "simulation/adcs_simulator.h"
#include "sun_sensor.h"

// TODO: figure out real necesary size.
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
        imu_sample_t imu_sample;
        magnetometer_sample_t magnetometer_sample;
        sun_sensor_sample_t sun_sample;
        uint32_t notification;

        memset(&packet, 0, sizeof(packet));
        (void)adcs_simulator_step((float)SENSOR_TASK_PERIOD_MS / 1000.0F);
        packet.sequence = ++sensor_sequence;
        packet.timestamp_us = adcs_simulator_get_time_us();

        if (imu_read(&imu_sample) == IMU_OK) {
            packet.timestamp_us = imu_sample.timestamp_us;
            memcpy(
                packet.angular_rate_rad_s,
                imu_sample.angular_rate_rad_s,
                sizeof(packet.angular_rate_rad_s));
            memcpy(
                packet.acceleration_m_s2,
                imu_sample.acceleration_m_s2,
                sizeof(packet.acceleration_m_s2));
            packet.valid_mask |= ADCS_SENSOR_VALID_GYROSCOPE |
                                 ADCS_SENSOR_VALID_ACCELEROMETER;
        }
        if (magnetometer_read(&magnetometer_sample) == MAGNETOMETER_OK) {
            memcpy(
                packet.magnetic_field_t,
                magnetometer_sample.magnetic_field_t,
                sizeof(packet.magnetic_field_t));
            packet.valid_mask |= ADCS_SENSOR_VALID_MAGNETOMETER;
        }
        if (sun_sensor_read(&sun_sample) == SUN_SENSOR_OK &&
            sun_sample.visible != 0U) {
            memcpy(
                packet.sun_vector_body,
                sun_sample.sun_vector_body,
                sizeof(packet.sun_vector_body));
            packet.sun_irradiance_w_m2 = sun_sample.irradiance_w_m2;
            packet.valid_mask |= ADCS_SENSOR_VALID_SUN;
        }
        adcs_manager_set_sensors(&packet);
        if (uxQueueMessagesWaiting(sensor_queue) != 0U) {
            adcs_manager_note_dropped_message();
        }
        (void)xQueueOverwrite(sensor_queue, &packet);

        if (xTaskNotifyWait(0U, UINT32_MAX, &notification, 0U) == pdTRUE) {
            printf("[SENSOR] Sampling sensors for move to position: %d\n",
                   (int32_t)notification);
            fflush(stdout);
        }
        xTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(SENSOR_TASK_PERIOD_MS));
    }
}
