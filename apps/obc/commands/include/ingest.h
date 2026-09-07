#ifndef OBC_COMMANDS_INGEST_H
#define OBC_COMMANDS_INGEST_H

#include "obc_ipc.h"
#include "csp_commands.h"

int ingest_thread_init(void);

void *ingest_thread(void *arg);

#endif // OBC_COMMANDS_INGEST_H
