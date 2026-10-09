#include "manager/fault_manager.h"

#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <csp/csp.h>

#include "board_reset.h"
#include "power_rail.h"

#define WATCHDOG_CHECK_PERIOD_SEC 1
#define WATCHDOG_TIMEOUT_SEC 5

static pthread_mutex_t pet_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t fault_lock = PTHREAD_MUTEX_INITIALIZER;
static struct timespec last_pet;
static eps_fault_config_t fault_config;
static uint32_t active_faults;
static atomic_uchar transport_enabled = 1U;

static eps_fault_config_t default_config(void) {
    return (eps_fault_config_t) {
        .minimum_pack_voltage_v = 11.0F,
        .maximum_pack_voltage_v = 17.0F,
        .maximum_pack_current_a = 5.0F,
        .maximum_temperature_c = 45.0F,
        .minimum_solar_irradiance_w_m2 = 50.0F,
        .sensor_stale_after_us = 500000U
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
        EPS_STATUS_PORT,
        1000,
        CSP_O_NONE);
    if (conn == NULL) {
        fprintf(stderr, "[EPS FAULT MGMT] failed to notify OBC of reset\n");
        return;
    }

    packet = csp_buffer_get(0);
    if (packet == NULL) {
        csp_close(conn);
        return;
    }

    notice.board_addr = EPS_ADDRESS;
    notice.reason = (uint8_t)reason;
    memcpy(packet->data, &notice, sizeof(notice));
    packet->length = sizeof(notice);
    (void)csp_send(conn, packet);
    csp_close(conn);
}

void fault_management_trigger_reset(reset_reason_t reason) {
    printf("[EPS FAULT MGMT] Resetting (reason=%d)\n", reason);
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
                    "[EPS FAULT MGMT] watchdog timeout -- housekeeping stopped petting\n");
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
    active_faults = EPS_FAULT_NONE;
    pthread_mutex_unlock(&fault_lock);
    clock_gettime(CLOCK_MONOTONIC, &last_pet);

    pthread_attr_init(&attributes);
    pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&thread, &attributes, watchdog_thread, NULL) != 0) {
        fprintf(stderr, "[EPS FAULT MGMT] failed to start watchdog thread\n");
    }
    pthread_attr_destroy(&attributes);
}

void fault_management_set_transport_enabled(uint8_t enabled) {
    atomic_store(&transport_enabled, enabled != 0U);
}

void fault_management_configure(const eps_fault_config_t *config) {
    if (config == NULL ||
        !isfinite(config->minimum_pack_voltage_v) ||
        !isfinite(config->maximum_pack_voltage_v) ||
        !isfinite(config->maximum_pack_current_a) ||
        !isfinite(config->maximum_temperature_c) ||
        !isfinite(config->minimum_solar_irradiance_w_m2) ||
        config->minimum_pack_voltage_v <= 0.0F ||
        config->maximum_pack_voltage_v <= config->minimum_pack_voltage_v ||
        config->maximum_pack_current_a <= 0.0F ||
        config->maximum_temperature_c <= 0.0F ||
        config->minimum_solar_irradiance_w_m2 < 0.0F ||
        config->sensor_stale_after_us == 0U) {
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

uint32_t fault_management_evaluate_eps(
    const eps_sensor_packet_t *sensors,
    uint64_t now_us) {
    eps_fault_config_t config;
    uint32_t faults = EPS_FAULT_NONE;

    pthread_mutex_lock(&fault_lock);
    config = fault_config;
    pthread_mutex_unlock(&fault_lock);

    if (sensors == NULL ||
        (sensors->valid_mask & EPS_SENSOR_VALID_BATTERY) == 0U ||
        sensors->timestamp_us == 0U ||
        sensors->timestamp_us > now_us ||
        now_us - sensors->timestamp_us > config.sensor_stale_after_us) {
        return EPS_FAULT_SENSOR_STALE;
    }

    if (!isfinite(sensors->pack_voltage_v) ||
        sensors->pack_voltage_v < config.minimum_pack_voltage_v) {
        faults |= EPS_FAULT_UNDER_VOLTAGE;
    }
    if (isfinite(sensors->pack_voltage_v) &&
        sensors->pack_voltage_v > config.maximum_pack_voltage_v) {
        faults |= EPS_FAULT_OVER_VOLTAGE;
    }
    if (!isfinite(sensors->pack_current_a) ||
        fabsf(sensors->pack_current_a) > config.maximum_pack_current_a) {
        faults |= EPS_FAULT_OVER_CURRENT;
    }
    if (!isfinite(sensors->battery_temperature_c) ||
        sensors->battery_temperature_c > config.maximum_temperature_c) {
        faults |= EPS_FAULT_OVER_TEMPERATURE;
    }

    if ((sensors->valid_mask & EPS_SENSOR_VALID_SOLAR) != 0U &&
        sensors->solar_irradiance_w_m2 < config.minimum_solar_irradiance_w_m2) {
        faults |= EPS_FAULT_SOLAR_FAILURE;
    }

    return faults;
}

void fault_management_report(eps_fault_t fault) {
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
