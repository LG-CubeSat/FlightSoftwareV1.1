#ifndef OBC_MISSION_HEARTBEAT_H
#define OBC_MISSION_HEARTBEAT_H

int heartbeat_thread_init(void);

void *heartbeat_thread(void *arg);

#endif // OBC_MISSION_HEARTBEAT_H
