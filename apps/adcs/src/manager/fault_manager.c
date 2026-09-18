#include "manager/fault_manager.h"

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdatomic.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <csp/csp.h>

#include "board_reset.h"
#include "control/control_math.h"

#define WATCHDOG_CHECK_PERIOD_SEC 1
#define WATCHDOG_TIMEOUT_SEC 5
#define MAXIMUM_SIM_DIPOLE_A_M2 1.0F
#define MAXIMUM_ESTIMATOR_RATE_RAD_S 100.0F
#define MAXIMUM_ESTIMATOR_COVARIANCE 1.0e6F

static pthread_mutex_t pet_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t fault_lock = PTHREAD_MUTEX_INITIALIZER;
static struct timespec last_pet;
static adcs_fault_config_t fault_config;
static uint32_t active_faults;
static atomic_uchar transport_enabled = 1U;

static adcs_fault_config_t default_config(void) {
    return (adcs_fault_config_t) {
        .maximum_rate_rad_s = 0.75F,
        .minimum_magnetic_field_t = 1.0e-6F,
        .maximum_magnetic_field_t = 1.0e-3F,
        .sensor_stale_after_us = 500000U,
        .estimate_stale_after_us = 500000U
    };
}

static void send_reset_notice(reset_reason_t reason) {
    csp_conn_t *conn;
    csp_packet_t *packet;
    board_reset_notice_t notice;

    if (atomic_load(&transport_enabled) == 0U) {
        return;
    }

    conn = csp_connect(
        CSP_PRIO_NORM,
        OBC_ADDRESS,
        ADCS_STATUS_PORT,
        1000,
        CSP_O_NONE);
    if (conn == NULL) {
        fprintf(stderr, "[FAULT MGMT] failed to notify OBC of reset\n");
        return;
    }

    packet = csp_buffer_get(0);
    if (packet == NULL) {
        csp_close(conn);
        return;
    }

    notice.board_addr = ADCS_ADDRESS;
    notice.reason = (uint8_t)reason;
    memcpy(packet->data, &notice, sizeof(notice));
    packet->length = sizeof(notice);
    (void)csp_send(conn, packet);
    csp_close(conn);
}

void fault_management_trigger_reset(reset_reason_t reason) {
    printf("[FAULT MGMT] Resetting (reason=%d)\n", reason);
    fflush(stdout);
    send_reset_notice(reason);
    board_reset();
}

static void *watchdog_thread(void *arg) {
    (void)arg;

    for (;;) {
        struct timespec seen;
        struct timespec now;
        double elapsed;

        sleep(WATCHDOG_CHECK_PERIOD_SEC);
        pthread_mutex_lock(&pet_lock);
        seen = last_pet;
        pthread_mutex_unlock(&pet_lock);

        clock_gettime(CLOCK_MONOTONIC, &now);
        elapsed = (double)(now.tv_sec - seen.tv_sec) +
                  (double)(now.tv_nsec - seen.tv_nsec) / 1.0e9;
        if (elapsed > WATCHDOG_TIMEOUT_SEC) {
            fprintf(stderr,
                    "[FAULT MGMT] watchdog timeout -- housekeeping stopped petting\n");
            fault_management_trigger_reset(RESET_REASON_WATCHDOG);
        }
    }
    return NULL;
}

void fault_management_init(void) {
    pthread_t thread;
    pthread_attr_t attributes;

    pthread_mutex_lock(&fault_lock);
    fault_config = default_config();
    active_faults = ADCS_FAULT_NONE;
    pthread_mutex_unlock(&fault_lock);
    clock_gettime(CLOCK_MONOTONIC, &last_pet);

    pthread_attr_init(&attributes);
    pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&thread, &attributes, watchdog_thread, NULL) != 0) {
        fprintf(stderr, "[FAULT MGMT] failed to start watchdog thread\n");
    }
    pthread_attr_destroy(&attributes);
}

void fault_management_set_transport_enabled(uint8_t enabled) {
    atomic_store(&transport_enabled, enabled != 0U);
}

void fault_management_configure(const adcs_fault_config_t *config) {
    if (config == NULL || !isfinite(config->maximum_rate_rad_s) ||
        !isfinite(config->minimum_magnetic_field_t) ||
        !isfinite(config->maximum_magnetic_field_t) ||
        config->maximum_rate_rad_s <= 0.0F ||
        config->minimum_magnetic_field_t <= 0.0F ||
        config->maximum_magnetic_field_t <= config->minimum_magnetic_field_t ||
        config->sensor_stale_after_us == 0U ||
        config->estimate_stale_after_us == 0U) {
        return;
    }

    pthread_mutex_lock(&fault_lock);
    fault_config = *config;
    pthread_mutex_unlock(&fault_lock);
}

void fault_management_pet(void) {
    pthread_mutex_lock(&pet_lock);
    clock_gettime(CLOCK_MONOTONIC, &last_pet);
    pthread_mutex_unlock(&pet_lock);
}

int fault_management_check_estimator_bounds(
    const adcs_attitude_state_t *attitude) {
    float quaternion_norm_squared = 0.0F;
    float rate_norm;

    if (attitude == NULL || attitude->timestamp_us == 0U) {
        return 0;
    }
    if (!adcs_values_are_finite(attitude->quaternion, 4U) ||
        !adcs_values_are_finite(
            attitude->angular_rate_rad_s,
            ADCS_VECTOR_LENGTH) ||
        !adcs_values_are_finite(
            attitude->gyro_bias_rad_s,
            ADCS_VECTOR_LENGTH) ||
        !adcs_values_are_finite(
            attitude->covariance_diagonal,
            ADCS_ERROR_STATE_LENGTH) ||
        !isfinite(attitude->confidence)) {
        return 1;
    }

    for (size_t index = 0U; index < 4U; ++index) {
        quaternion_norm_squared +=
            attitude->quaternion[index] * attitude->quaternion[index];
    }
    rate_norm = adcs_vector_norm(attitude->angular_rate_rad_s);
    if (!isfinite(quaternion_norm_squared) ||
        quaternion_norm_squared < 0.25F ||
        quaternion_norm_squared > 2.25F ||
        !isfinite(rate_norm) || rate_norm > MAXIMUM_ESTIMATOR_RATE_RAD_S ||
        attitude->confidence < 0.0F || attitude->confidence > 1.0F) {
        return 1;
    }
    for (size_t index = 0U; index < ADCS_ERROR_STATE_LENGTH; ++index) {
        if (attitude->covariance_diagonal[index] < 0.0F ||
            attitude->covariance_diagonal[index] > MAXIMUM_ESTIMATOR_COVARIANCE) {
            return 1;
        }
    }
    return 0;
}

uint32_t fault_management_evaluate_adcs(
    const adcs_sensor_packet_t *sensors,
    const adcs_attitude_state_t *attitude,
    const adcs_control_output_t *control,
    uint64_t now_us) {
    adcs_fault_config_t config;
    uint32_t faults = ADCS_FAULT_NONE;
    float magnetic_norm;
    float rate_norm;
    float quaternion_norm = 0.0F;

    pthread_mutex_lock(&fault_lock);
    config = fault_config;
    pthread_mutex_unlock(&fault_lock);

    if (sensors == NULL || sensors->timestamp_us == 0U ||
        sensors->timestamp_us > now_us ||
        now_us - sensors->timestamp_us > config.sensor_stale_after_us) {
        faults |= ADCS_FAULT_SENSOR_STALE;
    } else {
        uint32_t required = ADCS_SENSOR_VALID_GYROSCOPE |
                            ADCS_SENSOR_VALID_MAGNETOMETER;
        if ((sensors->valid_mask & required) != required ||
            !adcs_values_are_finite(
                sensors->angular_rate_rad_s,
                ADCS_VECTOR_LENGTH) ||
            !adcs_values_are_finite(
                sensors->magnetic_field_t,
                ADCS_VECTOR_LENGTH)) {
            faults |= ADCS_FAULT_SENSOR_RANGE;
        } else {
            magnetic_norm = adcs_vector_norm(sensors->magnetic_field_t);
            if (!isfinite(magnetic_norm) ||
                magnetic_norm < config.minimum_magnetic_field_t ||
                magnetic_norm > config.maximum_magnetic_field_t) {
                faults |= ADCS_FAULT_SENSOR_RANGE;
            }
        }
    }

    if (attitude == NULL || attitude->valid == 0U ||
        attitude->timestamp_us == 0U || attitude->timestamp_us > now_us ||
        now_us - attitude->timestamp_us > config.estimate_stale_after_us ||
        !adcs_values_are_finite(attitude->quaternion, 4U) ||
        !adcs_values_are_finite(
            attitude->angular_rate_rad_s,
            ADCS_VECTOR_LENGTH)) {
        faults |= ADCS_FAULT_ATTITUDE_INVALID;
    } else {
        for (size_t index = 0U; index < 4U; ++index) {
            quaternion_norm += attitude->quaternion[index] *
                               attitude->quaternion[index];
        }
        quaternion_norm = sqrtf(quaternion_norm);
        if (!isfinite(quaternion_norm) || fabsf(quaternion_norm - 1.0F) > 0.05F ||
            !isfinite(attitude->confidence) || attitude->confidence < 0.0F ||
            attitude->confidence > 1.0F) {
            faults |= ADCS_FAULT_ATTITUDE_INVALID;
        }

        rate_norm = adcs_vector_norm(attitude->angular_rate_rad_s);
        if (!isfinite(rate_norm) || rate_norm > config.maximum_rate_rad_s) {
            faults |= ADCS_FAULT_EXCESSIVE_RATE;
        }
    }

    if (control != NULL && control->actuators_enabled != 0U) {
        if (!adcs_values_are_finite(
                control->requested_torque_nm,
                ADCS_VECTOR_LENGTH) ||
            !adcs_values_are_finite(
                control->requested_dipole_a_m2,
                ADCS_VECTOR_LENGTH) ||
            !adcs_values_are_finite(
                control->achievable_torque_nm,
                ADCS_VECTOR_LENGTH)) {
            faults |= ADCS_FAULT_ACTUATOR;
        }
        for (size_t axis = 0U; axis < ADCS_VECTOR_LENGTH; ++axis) {
            if (fabsf(control->requested_dipole_a_m2[axis]) >
                    MAXIMUM_SIM_DIPOLE_A_M2) {
                faults |= ADCS_FAULT_ACTUATOR;
            }
        }
    }
    return faults;
}

void fault_management_report(adcs_fault_t fault) {
    pthread_mutex_lock(&fault_lock);
    active_faults |= (uint32_t)fault;
    pthread_mutex_unlock(&fault_lock);
}

uint32_t fault_management_get_active(void) {
    uint32_t snapshot;

    pthread_mutex_lock(&fault_lock);
    snapshot = active_faults;
    pthread_mutex_unlock(&fault_lock);
    return snapshot;
}

void fault_management_clear(uint32_t fault_mask) {
    pthread_mutex_lock(&fault_lock);
    active_faults &= ~fault_mask;
    pthread_mutex_unlock(&fault_lock);
}
