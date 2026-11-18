#ifndef ADCS_MUTEX_H
#define ADCS_MUTEX_H
#ifdef ADCS_STM32
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
typedef struct { StaticSemaphore_t storage; SemaphoreHandle_t handle; } adcs_mutex_t;
#define ADCS_MUTEX_INITIALIZER {0}
/* Bootstrap runs in main before any task can access the state. */
static inline void adcs_mutex_lock(adcs_mutex_t *mutex) {
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) return;
    taskENTER_CRITICAL();
    if (mutex->handle == NULL) mutex->handle = xSemaphoreCreateMutexStatic(&mutex->storage);
    taskEXIT_CRITICAL();
    configASSERT(mutex->handle != NULL);
    configASSERT(xSemaphoreTake(mutex->handle, portMAX_DELAY) == pdTRUE);
}
static inline void adcs_mutex_unlock(adcs_mutex_t *mutex) {
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) return;
    configASSERT(xSemaphoreGive(mutex->handle) == pdTRUE);
}
#else
#include <pthread.h>
typedef pthread_mutex_t adcs_mutex_t;
#define ADCS_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#define adcs_mutex_lock pthread_mutex_lock
#define adcs_mutex_unlock pthread_mutex_unlock
#endif
#endif
