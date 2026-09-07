#ifndef OBC_MISSION_AUTONOMY_H
#define OBC_MISSION_AUTONOMY_H

int init_autonomy_thread(void);

void *autonomy_thread(void *arg);

#endif // OBC_MISSION_AUTONOMY_H
