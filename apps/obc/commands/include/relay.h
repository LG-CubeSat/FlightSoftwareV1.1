#ifndef OBC_COMMANDS_RELAY_H
#define OBC_COMMANDS_RELAY_H

int relay_thread_init(void);

void *relay_thread(void *arg);

#endif // OBC_COMMANDS_RELAY_H
