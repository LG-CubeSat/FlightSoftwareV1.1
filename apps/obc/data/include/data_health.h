#ifndef OBC_DATA_HEALTH_H
#define OBC_DATA_HEALTH_H

/* Initializes Data's storage progress watch. Returns 0 on success. */
int data_health_init(void);

/* Records a completed storage polling or file-processing checkpoint. */
void data_health_storage_progress(void);

/* Data is healthy while its storage thread continues making progress. */
int data_health_is_healthy(void);

#endif
