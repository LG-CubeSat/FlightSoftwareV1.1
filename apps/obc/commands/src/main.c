#include <stdio.h>

#include "relay.h"
#include "ingest.h"
#include "csp/csp_network.h"
#include "csp_commands.h"
#include "obc_ipc.h"

int main(void) {
    printf("[OBC COMMAND P] Program started.\n");

    /*
    init stuff here
    */
    csp_network_init(OBC_ADDRESS, 1); // is master
    IPC_initialize(ROLE_COMMANDS);

    ingest_thread_init();
    relay_thread_init();

    /*
    Set off the threads
    */
    for (;;) { sleep(1); }

    return 0;
}
