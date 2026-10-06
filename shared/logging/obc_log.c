#include "obc_log.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

static char *g_component;
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;

int obc_log_init(const char *component)
{
    const char *path = getenv("OBC_LOG_PATH");

    snprintf(
        g_component,
        sizeof(g_component),
        "%s",
        component != NULL ? component : "unknown"
    );

    if (path != NULL && path[0] != '\0') {
        int fd = open(
            path,
            O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, // o_cloexec isolates this file descriptor to one process
            0640
        );

        if (fd < 0) {
            return -1;
        }

        if (dup2(fd, STDOUT_FILENO) < 0 ||
            dup2(fd, STDERR_FILENO) < 0) {
            close(fd);
            return -1;
        }

        if (fd > STDERR_FILENO) {
            close(fd);
        }
    }

    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    return 0;
}

void obc_log_write(obc_log_level_t level, const char *format)
{
    char timestamp[32];
    char message[768];
    char line[1024];

    struct timespec now;
    struct tm utc;

    clock_gettime(CLOCK_REALTIME, &now);
    gmtime_r(&now.tv_sec, &utc);

    strftime(
        timestamp,
        sizeof(timestamp),
        "%Y-%m-%dT%H:%M:%S",
        &utc
    );

    va_list arguments;
    va_start(arguments, format);
    vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);

    int length = snprintf(
        line, 
        sizeof(line),
        "%s.%03ldZ [%s] [pid=%ld] [%s] %s\n",
        timestamp,
        now.tv_nsec / 1000000L,
        g_component,
        (long)getpid(),
        level_name(level),
        message
    );

    if (length > 0) {
        size_t write_length = (size_t)length;

        if (write_length >= sizeof(line)) {
            write_length = sizeof(line) - 1;
        }

        pthread_mutex_lock(&g_log_mutex);
        write(STDERR_FILENO, line, write_length);
        pthread_mutex_unlock(&g_log_mutex);
    }
}