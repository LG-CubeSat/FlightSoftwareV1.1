#ifndef OBC_FDIR_HEALTH_MONITOR_H
#define OBC_FDIR_HEALTH_MONITOR_H

#include <time.h>

#include "obc_ipc.h"

int health_monitor_thread_init(void);
void *health_monitor_thread(void *arg);

int health_monitor_get_last_heartbeat(
    OBC_Roles_t role,
    struct timespec *last_seen_out
);

#endif // OBC_FDIR_HEALTH_MONITOR_H
