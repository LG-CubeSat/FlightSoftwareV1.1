#include "health_monitor.h"

#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#include "obc_ipc.h"
#include "csp_commands.h"
#include "fallback.h"

/*
Watches the health of each subsystem:
Turns the flags given from watchdog and limit checker into real issue states (how severe the problem is)
*/

#define RESET_SHUTDOWN_THRESHOLD 3

static int reset_counts[COMMS + 1]; // indexed by board address (OBC_ADDRESS unused)

/* Each board's own command port, so a shutdown goes to the right
   place -- add a line here as boards come online, same idea as
   ingest.c's routing table. */
static uint8_t cmd_port_for_board(uint8_t board_addr)
{
    switch (board_addr) {
        case ADCS_ADDRESS: return ADCS_CMD_PORT;
        case EPS_ADDRESS:  return EPS_CMD_PORT;
        default:           return 0;
    }
}

int health_monitor_thread_init(void)
{
    printf("[HEALTH MONITOR] Attempting Thread Init.\n");
    pthread_t health_monitor_pthread;

    int ret = pthread_create(&health_monitor_pthread, NULL, health_monitor_thread, NULL);
    if (ret != 0) {
        fprintf(stderr, "[HEALTH MONITOR] Thread failed to create: %d\n", ret);
    } else {
        printf("[HEALTH MONITOR] Init Successful.\n");
    }
    return ret;
}

void *health_monitor_thread(void *arg)
{
    (void)arg;

    for (;;) {
        OBC_Roles_t src;
        board_reset_notice_t notice;
        int len = IPC_receive(&src, (uint8_t *)&notice, sizeof(notice));
        if (len != sizeof(notice) || notice.board_addr > COMMS) {
            continue; // malformed or out-of-range address, ignore
        }

        reset_counts[notice.board_addr]++;
        printf("[HEALTH MONITOR] board %d reset (reason %d), count=%d\n",
               notice.board_addr, notice.reason, reset_counts[notice.board_addr]);
        fflush(stdout);

        // fires exactly once, on the cycle that crosses the threshold
        // SHOULD add an eps repower logic that lets operator manually force boot a board back up, so shutdown isn't permanent.
        if (reset_counts[notice.board_addr] == RESET_SHUTDOWN_THRESHOLD) {
            printf("[HEALTH MONITOR] board %d exceeded reset threshold, shutting it down\n",
                   notice.board_addr);
            fflush(stdout);

            // A reset notice means the board is about to briefly go dark
            // (it sends this, then resets itself) -- send the shutdown too
            // soon and it races the board's own reconnect, landing on no
            // live connection at all. This delay is a blunt fix for that.
            // OPTIONAL TODO: replace with relay/transport-level retry so
            // this doesn't depend on a fixed guess at reconnect time.
            sleep(2);

            fallback_shutdown_board(notice.board_addr, cmd_port_for_board(notice.board_addr));
        }
    }

    return NULL;
}
