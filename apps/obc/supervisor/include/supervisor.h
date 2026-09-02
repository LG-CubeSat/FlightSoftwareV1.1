#ifndef OBC_SUPERVISOR_H
#define OBC_SUPERVISOR_H

int init_supervisor(void);

int init_heartbeat_init_thread(void);

void *heartbeat_thread(void *arg);

int init_shutdown_thread(void);

void *shutdown_thread(void *arg);

#endif // OBC_SUPERVISOR_H