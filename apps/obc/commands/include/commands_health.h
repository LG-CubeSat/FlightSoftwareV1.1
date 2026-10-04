#ifndef OBC_COMMANDS_HEALTH_H
#define OBC_COMMANDS_HEALTH_H

/* Initializes Commands' ingest and relay progress watches. */
int commands_health_init(void);

void commands_health_ingest_progress(void);
void commands_health_relay_progress(void);

/* Commands is healthy only while both worker loops remain responsive. */
int commands_health_is_healthy(void);

#endif
