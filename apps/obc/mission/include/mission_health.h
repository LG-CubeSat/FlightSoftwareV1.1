#ifndef OBC_MISSION_HEALTH_H
#define OBC_MISSION_HEALTH_H

/* Initializes Mission's scheduler and autonomy progress watches. */
int mission_health_init(void);

void mission_health_scheduler_progress(void);
void mission_health_payload_begin(void);
void mission_health_payload_progress(void);
void mission_health_payload_end(void);
void mission_health_autonomy_progress(void);

/* Mission is healthy only while all active work continues progressing. */
int mission_health_is_healthy(void);

#endif
