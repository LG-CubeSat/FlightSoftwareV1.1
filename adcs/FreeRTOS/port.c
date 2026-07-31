#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

/* 
 * ROBUST POSIX SIMULATOR PORT (v2)
 * Handles lock yielding to prevent starvation.
 */

static pthread_mutex_t xKernelLock = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    TaskFunction_t pxCode;
    void *pvParameters;
} ThreadArgs_t;

// This function is called by the FreeRTOS kernel whenever it needs to "Switch"
void vPortYield(void) {
    pthread_mutex_unlock(&xKernelLock);
    // Tiny sleep to allow the scheduler thread or other tasks to grab the lock
    usleep(10); 
    pthread_mutex_lock(&xKernelLock);
}

static void *prvTaskEntry(void *pvParameters) {
    ThreadArgs_t *pxArgs = (ThreadArgs_t *)pvParameters;
    
    // Each task starts with the lock held
    pthread_mutex_lock(&xKernelLock);
    pxArgs->pxCode(pxArgs->pvParameters);
    pthread_mutex_unlock(&xKernelLock);
    
    free(pxArgs);
    return NULL;
}

StackType_t *pxPortInitialiseStack(StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters) {
    ThreadArgs_t *pxArgs = malloc(sizeof(ThreadArgs_t));
    pxArgs->pxCode = pxCode;
    pxArgs->pvParameters = pvParameters;

    pthread_t hThread;
    pthread_create(&hThread, NULL, prvTaskEntry, pxArgs);
    
    return pxTopOfStack;
}

BaseType_t xPortStartScheduler(void) {
    printf("[SIMULATOR] Scheduler Started. Yielding to tasks...\n");
    
    while(1) {
        pthread_mutex_lock(&xKernelLock);
        xTaskIncrementTick();
        pthread_mutex_unlock(&xKernelLock);
        
        // Wait for 1 tick period (1ms if 1000Hz)
        usleep(1000000 / configTICK_RATE_HZ);
    }
    return pdTRUE;
}

void vPortEndScheduler(void) {}
