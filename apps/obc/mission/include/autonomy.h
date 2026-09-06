#ifndef OBC_MISSION_AUTONOMY_H
#define OBC_MISSION_AUTONOMY_H

typedef struct {
    const char *name;
    int interval_sec;
    struct timespec last_fired;
    int (*action)(void);
} autonomy_action_t;

int init_autonomy_thread(void);

void *autonomy_thread(void *arg);

#endif // OBC_MISSION_AUTONOMY_H
