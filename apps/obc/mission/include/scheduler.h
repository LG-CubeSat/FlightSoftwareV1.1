#ifndef OBC_MISSION_SCHEDULER_H
#define OBC_MISSION_SCHEDULER_H

int init_scheduler_thread(void);

void *scheduler_thread(void *arg);


#endif // OBC_MISSION_SCHEDULER_H
