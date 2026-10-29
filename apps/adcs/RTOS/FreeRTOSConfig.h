#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H
#include <stdint.h>
extern uint32_t SystemCoreClock;
void adcs_assert_failed(const char *file, int line);
#define configUSE_PREEMPTION 1
#define configCPU_CLOCK_HZ SystemCoreClock
#define configTICK_RATE_HZ 1000U
#define configMAX_PRIORITIES 6
#define configMINIMAL_STACK_SIZE 128U
#define configMAX_TASK_NAME_LEN 16
#define configTICK_TYPE_WIDTH_IN_BITS TICK_TYPE_WIDTH_32_BITS
#define configSUPPORT_STATIC_ALLOCATION 1
#define configSUPPORT_DYNAMIC_ALLOCATION 0
#define configUSE_MUTEXES 1
#define configUSE_RECURSIVE_MUTEXES 1
#define configUSE_COUNTING_SEMAPHORES 1
#define configUSE_TASK_NOTIFICATIONS 1
#define configUSE_TIMERS 0
#define configUSE_TRACE_FACILITY 0
#define configUSE_IDLE_HOOK 0
#define configUSE_TICK_HOOK 0
#define configCHECK_FOR_STACK_OVERFLOW 2
#define configUSE_NEWLIB_REENTRANT 1
#define configQUEUE_REGISTRY_SIZE 0
#define configPRIO_BITS 4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY (15U << 4U)
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (5U << 4U)
#define INCLUDE_vTaskDelay 1
#define INCLUDE_xTaskDelayUntil 1
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_vTaskDelete 1
#define INCLUDE_xTaskGetSchedulerState 1
#define INCLUDE_xTaskGetCurrentTaskHandle 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define configASSERT(x) do { if (!(x)) adcs_assert_failed(__FILE__, __LINE__); } while (0)
/* The port owns SVC/PendSV; HAL and FreeRTOS share SysTick. */
#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler
#endif
