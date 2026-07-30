#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

/* 
 * PROFESSIONAL POSIX SIMULATOR PORT
 * This port creates a real pthread for every FreeRTOS task.
 * It uses a global mutex to simulate a single-core CPU.
 */

static pthread_mutex_t xKernelLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t xTickCond = PTHREAD_COND_INITIALIZER;

// Structure to pass to the pthread
typedef struct {
    TaskFunction_t pxCode;
    void *pvParameters;
} ThreadArgs_t;

// The actual thread wrapper
static void *prvTaskEntry(void *pvParameters) {
    ThreadArgs_t *pxArgs = (ThreadArgs_t *)pvParameters;
    
    // Wait for the scheduler to start
    pthread_mutex_lock(&xKernelLock);
    
    // Execute the FreeRTOS Task
    pxArgs->pxCode(pxArgs->pvParameters);
    
    pthread_mutex_unlock(&xKernelLock);
    return NULL;
}

StackType_t *pxPortInitialiseStack(StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters) {
    // In the simulator, we spawn the thread immediately when the stack is "initialized"
    // (This is a hack for the simplified simulator)
    ThreadArgs_t *pxArgs = malloc(sizeof(ThreadArgs_t));
    pxArgs->pxCode = pxCode;
    pxArgs->pvParameters = pvParameters;

    pthread_t hThread;
    pthread_create(&hThread, NULL, prvTaskEntry, pxArgs);
    
    return pxTopOfStack;
}

BaseType_t xPortStartScheduler(void) {
    printf("[SIMULATOR] Scheduler Engine Started.\n");
    
    while(1) {
        pthread_mutex_lock(&xKernelLock);
        
        // Increment the RTOS tick
        if (xTaskIncrementTick() != pdFALSE) {
            // In a real port, we'd check for context switches here
        }
        
        pthread_mutex_unlock(&xKernelLock);
        
        // Sleep for 1 tick (usually 1ms)
        usleep(1000000 / configTICK_RATE_HZ);
    }
    return pdTRUE;
}

void vPortEndScheduler(void) {}
