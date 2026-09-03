#ifndef OBC_COMMANDS_INGEST_H
#define OBC_COMMANDS_INGEST_H

int ingest_thread_init(void);

void *ingest_thread(void *arg);

#endif // OBC_COMMANDS_INGEST_H
