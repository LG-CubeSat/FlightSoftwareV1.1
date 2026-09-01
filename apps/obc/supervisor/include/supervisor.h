#ifndef OBC_SUPERVISOR_H
#define OBC_SUPERVISOR_H

int init_supervisor(void);

int heartbeat_init(void);

void heatbeat(void);

int shutdown_init(void);

void shutdown(void);

#endif // OBC_SUPERVISOR_H