#include "health_monitor.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
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

static struct timespec last_heartbeat[ROLE_COUNT];
static int heartbeat_seen[ROLE_COUNT];
static pthread_mutex_t role_health_lock = PTHREAD_MUTEX_INITIALIZER;

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

static int is_monitored_role(OBC_Roles_t role)
{
    switch (role) {
        case ROLE_COMMANDS:
        case ROLE_COMPUTE:
        case ROLE_DATA:
        case ROLE_MISSION:
        case ROLE_TIME:
            return 1;
        
        default:
            return 0;
    }
}

static void record_role_heartbeat(OBC_Roles_t role)
{
    if (!is_monitored_role(role)) {
        return;
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    // uses mutex because watchdog will read from last heartbeat
    pthread_mutex_lock(&role_health_lock);

    int first_heartbeat = !heartbeat_seen[role];
    last_heartbeat[role] = now;
    heartbeat_seen[role] = 1;

    pthread_mutex_unlock(&role_health_lock);

    if (first_heartbeat) {
        printf("[HEALTH MONITOR] first heartbeat received from role %d\n", role);
        fflush(stdout);
    }
}

int health_monitor_get_last_heartbeat(
    OBC_Roles_t role,
    struct timespec *last_seen_out
)
{
    if (!is_monitored_role(role) || last_seen_out == NULL) {
        return 0;
    }

    pthread_mutex_lock(&role_health_lock);

    int seen = heartbeat_seen[role];
    if (seen) {
        *last_seen_out = last_heartbeat[role];
    }

    pthread_mutex_unlock(&role_health_lock);

    return seen;
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
        uint8_t buffer[sizeof(board_reset_notice_t)];

        int len = IPC_receive(
            &src,
            buffer,
            sizeof(buffer)
        );

        /*
        A zero-payload message is an internal OBC heartbeat.
        The role identity comes from the IPC header through src
        */
        if (len == 0) {
            record_role_heartbeat(src);
            continue;
        }

        /*
        Board reset notices must have exactly the expected wire size
        */
        if (len != sizeof(board_reset_notice_t)) {
            continue;
        }
        
        /*
        If something wants to reset another board, it must come from the Commands. This prevents roles to spoof a board reset.
        */
        if (src != ROLE_COMMANDS) {
            fprintf(
                stderr,
                "[HEALTH MONITOR] ignoring reset notice from role %d\n",
                src
            );
            continue;
        }

        board_reset_notice_t notice;
        memcpy(&notice, buffer, sizeof(notice));

        if (notice.board_addr > COMMS) {
            continue;
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
