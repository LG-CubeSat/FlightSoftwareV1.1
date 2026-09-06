#ifndef OBC_TIME_TIME_SYNC_H
#define OBC_TIME_TIME_SYNC_H

int init_broadcast_thread(void);

void *broadcast_thread(void *arg);

int init_request_listener_thread(void);

void *request_listener_thread(void *arg);

#endif // OBC_TIME_TIME_SYNC_H
