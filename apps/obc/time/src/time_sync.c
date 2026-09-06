#include "time_sync.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pthread.h"
#include "csp_commands.h"
#include "obc_relay_protocol.h"
#include "obc_ipc.h"
#include "obc_sleep_until.h"
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

/* Basically sync is a constant periodic thread that updates */
int time_sync_broadcast_thread_init(void)
{
    printf("[TIME SYNC] Attempting to create pthread.\n");
    pthread_t time_sync_broadcast_pthread;
    int ret = pthread_create(&time_sync_broadcast_pthread, NULL, time_sync_broadcast_thread, NULL);
    if (ret != 0) {
        printf("[TIME SYNC] Failed to create pthread.\n");
    } else {
        printf("[TIME SYNC] Successfully created pthread.\n");
    }
    return ret;
}

void *time_sync_broadcast_thread(void *arg)
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

/* request is if the boards can't wait, then we can serve immediately.*/
int time_sync_request_thread_init(void)
{
    printf("[REQUEST LISTENER] Attempting to create pthread.\n");
    pthread_t time_sync_request_pthread;
    int ret = pthread_create(&time_sync_request_pthread, NULL, time_sync_request_thread, NULL);

    if (ret != 0) {
        printf("[REQUEST LISTENER] Failed to create pthread.\n");
    } else {
        printf("[REQUEST LISTENER] Successfully created pthread.\n");
    }
    return ret;
}

void *time_sync_request_thread(void *arg)
{
    (void)arg;
    uint32_t seq = 0;

    for (;;) {
        OBC_Roles_t src;
        uint8_t buf[sizeof(time_sync_request_t)];
        int len = IPC_receive(&src, buf, sizeof(buf));
        if (len != sizeof(time_sync_request_t)) continue;
    
        time_sync_request_t req;
        memcpy(&req, buf, sizeof(req));

        for (size_t i=0; i < sizeof(targets)/sizeof(targets[0]); i++) {
            if (targets[i].addr == req.requester_addr) {
                send_time_sync_to(targets[i].addr, targets[i].cmd_port, seq++);
                break;
            }
        }
    }
    
    return NULL;
}