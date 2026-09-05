#include "relay.h"

#include <stdio.h>

#include "pthread.h"
#include "stdint.h"
#include "obc_ipc.h"

int relay_thread_init()
{
    printf("[RELAY THREAD] Attempting Init.\n");

    pthread_t relay_pthread;
    
    int ret = pthread_create(&relay_pthread, NULL, relay_thread, NULL);
    if (ret != 0) {
        fprintf(stderr, "[RELAY THREAD] Failed to initialize: %d\n", ret);
    } else {
        printf("[RELAY THREAD] Successfully initialized.\n");
    }
    return ret;
}

void *relay_thread(void *arg) 
{
    (void)arg;
    
    for (;;) {
        OBC_Roles_t src;
        uint8_t buf[64];
        int len = IPC_recieve(&src, buf, sizeof(buf));
        if (len < 0) continue;
        printf("[RELAY] got %d bytes from role %d (not yet forwarded over CSP)\n", len, src);
    }
    return NULL;
}