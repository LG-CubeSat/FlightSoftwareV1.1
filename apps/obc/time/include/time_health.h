#ifndef OBC_TIME_HEALTH_H
#define OBC_TIME_HEALTH_H

/* Initializes Time's broadcast and request-listener progress watches. */
int time_health_init(void);

void time_health_broadcast_begin(void);
void time_health_broadcast_progress(void);
void time_health_broadcast_end(void);
void time_health_request_progress(void);

/* Time is healthy only while all currently active work is progressing. */
int time_health_is_healthy(void);

#endif
