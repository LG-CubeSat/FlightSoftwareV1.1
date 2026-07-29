#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "FreeRTOS.h"
#include "task.h"

/* Simplified POSIX Port for Simulation */

StackType_t *pxPortInitialiseStack(StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters) {
    return pxTopOfStack;
}

BaseType_t xPortStartScheduler(void) {
    while(1) {
        // In a real simulator, this would be triggered by a timer interrupt.
        // Here we just manually increment the tick and let the tasks run.
        if (xTaskIncrementTick() != pdFALSE) {
            // Task switch would happen here in a real port
        }
        usleep(1000000 / configTICK_RATE_HZ);
    }
    return pdTRUE;
}

void vPortEndScheduler(void) {}

/* Required when configSUPPORT_STATIC_ALLOCATION is 1 */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    uint32_t *pulIdleTaskStackSize ) {
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

/* Required when configUSE_TIMERS and configSUPPORT_STATIC_ALLOCATION are 1 */
void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer,
                                     StackType_t **ppxTimerTaskStackBuffer,
                                     uint32_t *pulTimerTaskStackSize ) {
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
