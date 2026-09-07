#ifndef OBC_TIME_TIME_SYNC_H
#define OBC_TIME_TIME_SYNC_H

int time_sync_broadcast_thread_init(void);

void *time_sync_broadcast_thread(void *arg);

int time_sync_request_thread_init(void);

void *time_sync_request_thread(void *arg);

#endif // OBC_TIME_TIME_SYNC_H
