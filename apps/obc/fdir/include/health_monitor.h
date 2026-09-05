#ifndef OBC_FDIR_HEALTH_MONITOR_H
#define OBC_FDIR_HEALTH_MONITOR_H

int health_monitor_thread_init(void);
void *health_monitor_thread(void *arg);

#endif // OBC_FDIR_HEALTH_MONITOR_H
