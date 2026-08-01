#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "communication/spi.h"
#include "communication/spi_task.h"
#include "commands/command_queue.h"
#include "commands/command_task.h"
#include "commands/command_handler.h"
#include "control/control_queue.h"
#include "control_task.h"

int main(void)
{
    printf("--- ADCS Flight Software simulator ----\n");


    printf("Starting FreeRTOS Scheudler...\n");

    adcs_context_initialize();
    spi_initialize(); // Connect to the Virtual Bus
    
    // initialize queues
    command_queue_initialize();
    control_queue_initialize();

    control_task_create_static();
    command_task_create_static();
    spi_task_create_static();

    vTaskStartScheduler();

    for(;;);
    return 0;
}

// The idle task is the lowest priority task that the CPU will run when nothing is required to do
/* Required by FreeRTOS when configSUPPORT_STATIC_ALLOCATION is 1 */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    uint32_t *pulIdleTaskStackSize )
{
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

/* Required by FreeRTOS when configSUPPORT_STATIC_ALLOCATION is 1 and configUSE_TIMERS is 1 */
void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer,
                                     StackType_t **ppxTimerTaskStackBuffer,
                                     uint32_t *pulTimerTaskStackSize )
{
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];

    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}