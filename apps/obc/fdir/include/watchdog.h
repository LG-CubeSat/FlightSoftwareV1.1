#ifndef OBC_FDIR_WATCHDOG_H
#define OBC_FDIR_WATCHDOG_H

int watchdog_thread_init(void);
void *watchdog_thread(void *arg);

#endif // OBC_FDIR_WATCHDOG_H
