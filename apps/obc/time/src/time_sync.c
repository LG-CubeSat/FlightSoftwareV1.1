#include "time_sync.h"

#include <stdio.h>
#include <stdint.h>
#include "pthread.h"
#include "csp_commands.h"
#include "obc_relay_protocol.h"
#include "obc_ipc.h"
#include "time.h"

#define TIME_SYNC_INTERVAL_SEC (300) // 5 min, tune later

typedef struct {
    uint8_t addr;
    uint8_t cmd_port;
} time_sync_target_t;

static const time_sync_target_t targets[] = {
    { ADCS_ADDRESS, ADCS_CMD_PORT },
    { EPS_ADDRESS, EPS_CMD_PORT },
    // EPS has no command_handler yet -- harmless no-op until it does
};

static int send_time_sync_to(uint8_t addr, uint8_t cmd_port, uint32_t seq)
{
    time_sync_command_t sync = {
        .envelope = { .command_id = CMD_TIME_SYNC, .seq = seq },
        .unix_time_sec = (int64_t)time(NULL)
    };

    relay_request_t req = { 
        .dest_addr = addr, 
        .dest_port = cmd_port,
        .length = sizeof(sync) 
    };

    memcpy(req.payload, &sync, sizeof(sync));
    return IPC_send(ROLE_COMMANDS, (const uint8_t *)&req, sizeof(req)) < 0 ? -1 : 0;
}

int time_sync_broadcast_thread_init(void)
{
    printf("[TIME SYNC] Attempting to create pthread.\n");
    pthread_t time_sync_broadcast_pthread;
    int ret = pthread_create(&time_sync_broadcast_pthread, NULL, broadcast_thread, NULL);
    if (ret != 0) {
        printf("[TIME SYNC] Failed to create pthread.\n");
    } else {
        printf("[TIME SYNC] Successfully created pthread.\n");
    }
    return ret;
}

static void *broadcast_thread(void *arg)
{
    (void)arg;
    uint32_t seq = 0;
    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);

    for (;;) {
        for (size_t i = 0; i < sizeof(targets)/sizeof(targets[0]); i++) {
            send_time_sync_to(targets[i].addr, targets[i].cmd_port, seq);
        }
        seq++;

        next.tv_sec += TIME_SYNC_INTERVAL_SEC;
        obc_sleep_until(&next);
    }
    return NULL;
}

