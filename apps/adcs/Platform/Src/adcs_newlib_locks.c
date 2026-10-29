/* Serialize newlib's global stdio state and UART output with one recursive
 * mutex. Per-task _reent alone does not protect FILE objects shared by tasks.
 * Using one lock also avoids stdio/UART lock-order inversions. */
#include <sys/lock.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "adcs_board.h"

struct __lock { unsigned char unused; };
struct __lock __lock___arc4random_mutex;
struct __lock __lock___at_quick_exit_mutex;
struct __lock __lock___atexit_recursive_mutex;
struct __lock __lock___dd_hash_mutex;
struct __lock __lock___env_recursive_mutex;
struct __lock __lock___malloc_recursive_mutex;
struct __lock __lock___sfp_recursive_mutex;
struct __lock __lock___tz_mutex;
static struct __lock shared_lock;
static StaticSemaphore_t storage;
static SemaphoreHandle_t mutex;

static void initialize_mutex(void) {
    taskENTER_CRITICAL();
    if (mutex == NULL) mutex = xSemaphoreCreateRecursiveMutexStatic(&storage);
    taskEXIT_CRITICAL();
    configASSERT(mutex != NULL);
}
void adcs_stdio_lock(void) {
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) return;
    initialize_mutex();
    configASSERT(xSemaphoreTakeRecursive(mutex, portMAX_DELAY) == pdTRUE);
}
void adcs_stdio_unlock(void) {
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) return;
    configASSERT(xSemaphoreGiveRecursive(mutex) == pdTRUE);
}
void __retarget_lock_init(_LOCK_T *lock) { *lock = &shared_lock; }
void __retarget_lock_init_recursive(_LOCK_T *lock) { *lock = &shared_lock; }
void __retarget_lock_close(_LOCK_T lock) { (void)lock; }
void __retarget_lock_close_recursive(_LOCK_T lock) { (void)lock; }
void __retarget_lock_acquire(_LOCK_T lock) { (void)lock; adcs_stdio_lock(); }
void __retarget_lock_acquire_recursive(_LOCK_T lock) { (void)lock; adcs_stdio_lock(); }
void __retarget_lock_release(_LOCK_T lock) { (void)lock; adcs_stdio_unlock(); }
void __retarget_lock_release_recursive(_LOCK_T lock) { (void)lock; adcs_stdio_unlock(); }
int __retarget_lock_try_acquire(_LOCK_T lock) {
    (void)lock;
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) return 1;
    initialize_mutex();
    return xSemaphoreTakeRecursive(mutex, 0) == pdTRUE;
}
int __retarget_lock_try_acquire_recursive(_LOCK_T lock) { return __retarget_lock_try_acquire(lock); }
