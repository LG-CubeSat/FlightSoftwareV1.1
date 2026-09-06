#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include "supervisor.h"
#include "processes.h"

static volatile sig_atomic_t g_shutdown_requested = 0;

// signal handler: shutsdown the supervisor
static void handle_sigterm(int sig)
{
    (void)sig;
    g_shutdown_requested = 1;
}

int main(void) {
    // build signal
    struct sigaction sa = {0}; // 0 inits everything to default vanilla
    sa.sa_handler = handle_sigterm; // pass in the actual sigterm
    sigemptyset(&sa.sa_mask); 
    sigaction(SIGTERM, &sa, NULL); // NULL can be replaced with a pointer to receive old sigaction. Not needed here.

    if (init_supervisor() != 0) {
        printf("[OBC SUPERVISOR] Failed to launch other sub-processes.\n");
        return 1;
    }

    if (init_heartbeat_thread() != 0) {
        printf("[OBC SUPERVISOR] Failed to launch the Heartbeat thread.\n");
    }

    if (init_shutdown_thread() != 0) {
        printf("[OBC SUPERVISOR] Failed to launch the Shutdown thread.\n");
    }

    // g_shutdown_requested is volatile to avoid caching
    while (!g_shutdown_requested) {
        sleep(1);
    }

    printf("[OBC SUPERVISOR] Shutdown requested, stopping all processes.\n");
    supervisor_shutdown_all();
    printf("[OBC SUPERVISOR] Clean shutdown complete.\n");

    return 0;
}
