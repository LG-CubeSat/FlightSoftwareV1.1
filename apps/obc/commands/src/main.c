#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "relay.h"
#include "ingest.h"
#include "heartbeat.h"
#include "csp_network.h"
#include "csp_commands.h"
#include "obc_ipc.h"
#include "obc_relay_protocol.h"
#include "commands_health.h"
#include "obc_log.h"

int main(void) {
    if (obc_log_init("command") != 0) {
        fprintf(stderr, "[OBC COMMAND] Failed to initialize logging.\n");
    }

    LOG_INFO("Initializing");

    printf("[OBC COMMAND] Program started.\n");

    /*
    init stuff here
    */
    csp_network_init(OBC_ADDRESS, 1); // is master
    if (IPC_initialize(ROLE_COMMANDS) != IPC_OK) {
        fprintf(stderr, "[OBC COMMANDS] Failed to initialize IPC.\n");
        return 1;
    }

    if (commands_health_init() != 0) {
        fprintf(stderr, "[OBC COMMANDS] Failed to initialize health tracking.\n");
        return 1;
    }

    if (ingest_thread_init() != 0 || relay_thread_init() != 0) {
        fprintf(stderr, "[OBC COMMANDS] Failed to start a worker thread.\n");
        return 1;
    }

    if (heartbeat_thread_init() != 0) {
        fprintf(stderr, "[OBC COMMANDS] Failed to start heartbeat thread.\n");
        return 1;
    }

    for (;;) { sleep(1); }

    return 0;
}
