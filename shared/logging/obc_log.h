#ifndef OBC_LOG_H
#define OBC_LOG_H

typedef enum {
    OBC_LOG_DEBUG,
    OBC_LOG_INFO,
    OBC_LOG_WARN,
    OBC_LOG_ERROR
} obc_log_level_t;

/*
initializes logging for this process
if OBC_log_path is set, stdout and stderr are redirected to that file in append mode.
otherwise outputs continues to the existing terminal or inherited destination
*/
int obc_log_init(const char *component);

void obc_log_write(
    obc_log_level_t level,
    const char *format,
    ... // so this is for variadic functions. Means more than one argument. Needed for strings. printf() uses too
);

#define LOG_DEBUG(...) obc_log_write(OBC_LOG_DEBUG, __VA_ARGS__)
#define LOG_INFO(...) obc_log_write(OBC_LOG_INFO, __VA_ARGS__)
#define LOG_WARN(...) obc_log_write(OBC_LOG_WARN, __VA_ARGS__)
#define LOG_ERROR(...) obc_log_write(OBC_LOG_ERROR, __VA_ARGS__)

#endif