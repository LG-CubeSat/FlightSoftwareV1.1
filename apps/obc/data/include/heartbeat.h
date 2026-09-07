#ifndef OBC_TIME_HEARTBEAT_H
#define OBC_TIME_HEARTBEAT_H

int heartbeat_thread_init(void);

void *heartbeat_thread(void *arg);

#endif // OBC_TIME_HEARTBEAT_H
