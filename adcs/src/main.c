#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "communication/packet_parser.h"
#include "communication/spi.h"
#include "commands/command_queue.h"
#include "commands/command_task.h"
#include "commands/command_handler.h"
#include "control_task.h"

int main(void)
{
    printf("--- ADCS Flight Software simulator ----\n");


    printf("Starting FreeRTOS Scheudler...\n");

    control_task_create_static();
    command_task_create_static();

    vTaskStartScheduler();

    for(;;);
    return 0;
    // printf("LG Cubesat ADCS starting...\n");

    // printf("Initializing Hardware...\n");

    // spi_initialize();

    // packet_parser_initialize();

    // command_queue_initialize();

    // command_task_initialize();

    // command_handler_initialize();

    // adcs_state_initialize();

    // printf("Starting RTOS...\n");

    // printf("ADCS ready.\n");

    // uint8_t exit_safe_packet[] = 
    // {
    //     0xAA, //sync
    //     0x02, // Desination: ADCS
    //     0x01, // Sequence
    //     0x04, // exit safe mode
    //     0x00, // payload length
    //     0x00, // CRC
    // };

    // uint8_t point_to_sun_packet[] = 
    // {
    //     0xAA, // sync
    //     0x02, // Destination: ADCS
    //     0x02, // Sequence
    //     0x01, // Command: POINT_TO_SUN
    //     0x00, // Payload length
    //     0x00, // CRC (fake for now)
    // };

    // printf("\nSimulating SPI reception...\n");
    
    // printf("\n Sending EXIT_SAFE_MODE...\n");
    // spi_recieve(
    //     exit_safe_packet,
    //     sizeof(exit_safe_packet)
    // );

    // command_task_run();

    // printf("\n Sending POINT_TO_SUN...\n");
    
    // spi_recieve(
    //     exit_safe_packet,
    //     sizeof(exit_safe_packet)
    // );

    // command_task_run();

    // printf("\nUpdating ADCS state...\n");

    // adcs_state_update();

    // return 0;
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