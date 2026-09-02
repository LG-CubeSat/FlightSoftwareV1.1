#include "ingest.h"

#include <stdio.h>

#include "pthread.h"

int ingest_thread_init()
{
    printf("[INGEST THREAD] Attempting Init.\n");
    pthread_t ingest_pthread;
    int ret = pthread_create(&ingest_pthread, NULL, ingest_thread, NULL);
    
    if (ret != 0) {
        fprintf(stderr, "[INGEST THREAD] Thread failed to create: %d\n", ret);
    } else {
        printf("[INGEST THREAD] Init Successful.\n");
    }
    return ret;
}

void *ingest_thread(void *arg)
{
    (void)arg;
    // TODO: detatch vs join (for safe shutdown)
    return NULL;
}