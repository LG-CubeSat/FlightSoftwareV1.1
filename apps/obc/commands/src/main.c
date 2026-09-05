#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "relay.h"
#include "ingest.h"
#include "csp_network.h"
#include "csp_commands.h"
#include "obc_ipc.h"
#include "obc_relay_protocol.h"

int main(void) {
    printf("[OBC COMMAND P] Program started.\n");

    /*
    init stuff here
    */
    csp_network_init(OBC_ADDRESS, 1); // is master
    IPC_initialize(ROLE_COMMANDS);

    ingest_thread_init();
    relay_thread_init();

    /* TEMPORARY TEST HOOK -- sends 3 out-of-bounds move commands, spaced
       out so ADCS has time to fault, notify, self-reset, and reconnect
       between each. Proves the full loop: local fault -> reset -> notice
       -> health_monitor counts -> shutdown on the 3rd. Remove once real
       fault sources exist. */
    for (int i = 0; i < 3; i++) {
        sleep(3);
        command_envelope_t cmd_env = { .command_id = CMD_MOVE_TO_POSITION, .seq = (uint32_t)i };
        position_command_t cmd = { .envelope = cmd_env, .target_position = 99999 };
        relay_request_t req = { .dest_addr = ADCS_ADDRESS, .dest_port = ADCS_CMD_PORT, .length = sizeof(cmd) };
        memcpy(req.payload, &cmd, sizeof(cmd));
        IPC_send(ROLE_COMMANDS, (uint8_t *)&req, sizeof(req));
        printf("[OBC COMMAND P] TEST: sent out-of-bounds move #%d\n", i + 1);
        fflush(stdout);
    }

    for (;;) { sleep(1); }

    return 0;
}
