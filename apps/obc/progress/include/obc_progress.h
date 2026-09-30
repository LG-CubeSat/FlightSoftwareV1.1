#ifndef OBC_PROGRESS_H
#define OBC_PROGRESS_H

#include <pthread.h>
#include <stdint.h>
#include <time.h>

/*
Tracks one activity:
idle = healthy indefinitely. There is no work that should be advancing.
active=healthy only hwile progress has occurered within timeout_ms.
*/

typedef struct {
    pthread_mutex_t lock;
    struct timespec last_progress;
    uint64_t timeout_ms;
    int active;
} obc_progress_t;

/*
Initializes a caller owned watch in the idle state
returns 0 on success or a pthread error code on failure.
*/
int obc_progress_init(obc_progress_t *watch, uint64_t timeout_ms);

/*
Marks an operation active and starts its progress deadline.
*/
void obc_progress_begin(obc_progress_t *watch);

/*
Records that the active operation passed a meaningful checkpoint.
*/
void obc_progress_touch(obc_progress_t *watch);

/*
Marks the operation idle. Idle watches are healthy.
*/
void obc_progress_end(obc_progress_t *watch);

/*
Returns 1 when healthy and 0 when progress is stale.
*/
int obc_progress_is_healthy(obc_progress_t *watch);

/*
Releases recourses owned by the watch
This is mostly for tests. Flight processes generally run until exit.
*/

void obc_progress_destroy(obc_progress_t *watch);

#endif