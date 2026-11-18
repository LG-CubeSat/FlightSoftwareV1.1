#include "adcs_board.h"
#include "main.h"
#include "adcs_log.h"
#include "FreeRTOS.h"
#include "task.h"
#include "board_reset.h"
#include "board_shutdown.h"
#include "magnetorquer.h"
#include <reent.h>

/* LSI nominal 32 kHz / 256, 4096 counts: about 32 seconds. The independent
 * watchdog still operates if the scheduler or interrupt handling stops. */
void adcs_watchdog_start(void) {
    IWDG->KR = 0xCCCCU;
    IWDG->KR = 0x5555U;
    IWDG->PR = 6U;
    IWDG->RLR = 4095U;
    while (IWDG->SR != 0U) { }
    IWDG->KR = 0xAAAAU;
}
void adcs_watchdog_pet(void) { IWDG->KR = 0xAAAAU; }
void board_reset(void) {
    (void)magnetorquer_disable();
    /* Give the I2C service task a chance to expose the ACK/reset notice. */
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) vTaskDelay(pdMS_TO_TICKS(100));
    NVIC_SystemReset();
}
void board_shutdown(void) {
    (void)magnetorquer_disable();
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) vTaskDelay(pdMS_TO_TICKS(100));
    __disable_irq();
    /* IWDG cannot be disabled once started; keep it fed during intentional halt. */
    for (;;) { adcs_watchdog_pet(); }
}
void adcs_assert_failed(const char *file, int line) {
    (void)file;
    (void)line;
    __disable_irq();
    /* Do not use printf or RTOS locks after a corrupt stack/assertion. IWDG resets. */
    for (;;) { }
}
void vApplicationStackOverflowHook(TaskHandle_t task, char *name) {
    (void)task;
    (void)name;
    adcs_assert_failed(__FILE__, __LINE__);
}
/* Newlib's per-task reentrancy does not protect its global allocator. */
void __malloc_lock(struct _reent *reent) {
    (void)reent;
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) vTaskSuspendAll();
}
void __malloc_unlock(struct _reent *reent) {
    (void)reent;
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) (void)xTaskResumeAll();
}
