#include "fault_manager.h"

#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <csp/csp.h>
#include "board_reset.h"

#define WATCHDOG_CHECK_PERIOD_SEC 1
#define WATCHDOG_TIMEOUT_SEC 5
#define POSITION_LIMIT 1000 // placeholder bound, tune once real limits are known

static pthread_mutex_t pet_lock = PTHREAD_MUTEX_INITIALIZER;
static struct timespec last_pet;

static void send_reset_notice(reset_reason_t reason)
{
    csp_conn_t *conn = csp_connect(CSP_PRIO_NORM, OBC_ADDRESS, ADCS_STATUS_PORT, 1000, CSP_O_NONE);
    if (conn == NULL) {
        fprintf(stderr, "[FAULT MGMT] failed to notify OBC of reset\n");
        return;
    }

    csp_packet_t *packet = csp_buffer_get(0);
    if (packet == NULL) {
        csp_close(conn);
        return;
    }

    board_reset_notice_t notice = { .board_addr = ADCS_ADDRESS, .reason = (uint8_t)reason };
    memcpy(packet->data, &notice, sizeof(notice));
    packet->length = sizeof(notice);

    csp_send(conn, packet);
    csp_close(conn);
}

void fault_management_trigger_reset(reset_reason_t reason)
{
    printf("[FAULT MGMT] Resetting (reason=%d)\n", reason);
    fflush(stdout);

    send_reset_notice(reason); // best effort -- a hard crash means this never got out either

    board_reset(); // never returns -- simulated via execv, or a real MCU reset under HW_MODE
}

static void *watchdog_thread(void *arg)
{
    (void)arg;
    for (;;) {
        sleep(WATCHDOG_CHECK_PERIOD_SEC);

        pthread_mutex_lock(&pet_lock);
        struct timespec seen = last_pet;
        pthread_mutex_unlock(&pet_lock);

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double elapsed = (now.tv_sec - seen.tv_sec) + (now.tv_nsec - seen.tv_nsec) / 1e9;

        if (elapsed > WATCHDOG_TIMEOUT_SEC) {
            fprintf(stderr, "[FAULT MGMT] watchdog timeout -- housekeeping stopped petting\n");
            fault_management_trigger_reset(RESET_REASON_WATCHDOG);
        }
    }
    return NULL;
}

void fault_management_init(void)
{
    clock_gettime(CLOCK_MONOTONIC, &last_pet);

    // deliberately a raw pthread, not a FreeRTOS task: this has to stay
    // alive and scheduled by the host OS even if FreeRTOS's own
    // scheduler is the thing that's hung.
    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, watchdog_thread, NULL);
    pthread_attr_destroy(&attr);
}

void fault_management_pet(void)
{
    pthread_mutex_lock(&pet_lock);
    clock_gettime(CLOCK_MONOTONIC, &last_pet);
    pthread_mutex_unlock(&pet_lock);
}

/* THIS IS WHERE YOU WOULD ADD MORE CONDITIONS TO CHECK */
int fault_management_check_bounds(int32_t position)
{
    return (position > POSITION_LIMIT || position < -POSITION_LIMIT) ? 1 : 0;
}
