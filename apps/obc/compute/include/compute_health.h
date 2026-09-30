#ifndef OBC_COMPUTE_HEALTH_H
#define OBC_COMPUTE_HEALTH_H

/*
Initializes compute's dispatch and worker progress watches.
Returns 0 on success.
*/
int compute_health_init(void);


/* called whenever dispatch loop completes an iteration */
void compute_health_dispatch_progress(void);

/* Worker Lifecycle */
void compute_health_worker_begin(void);
void compute_health_worker_progress(void);
void compute_health_worker_end(void);

/*
Compute is healthy only if both its dispatch and worker watches are healthy
*/
int compute_healthy(void);

#endif