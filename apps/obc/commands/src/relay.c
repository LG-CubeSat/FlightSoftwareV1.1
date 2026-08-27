#include "relay.h"

#include <stdio.h>

#include "pthread.h"

int relay_thread_init()
{
    printf("[RELAY THREAD] Attempting Init.\n");

    pthread_t relay_thread;
    
    int ret = pthread_create(&relay_thread, NULL, relay_thread, NULL);
    if (ret != 0) {
        fprintf(stderr, "[RELAY THREAD] Failed to initialize: %d\n", ret);
    } else {
        printf("[RELAY THREAD] Successfully initialized.\n");
    }
}

int relay_thread() {

}