#include "board_reset.h"

#include <unistd.h>
#include <limits.h>
#include <stdio.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

/* Finds this binary's own path so a "reset" can re-exec it. Same idea
   as the OBC supervisor's obc_resolve_sibling, kept separate since
   these are different build targets (board vs OBC process). */
static int resolve_self_path(char *out, size_t out_size)
{
#if defined(__APPLE__)
    uint32_t size = (uint32_t)out_size;
    return _NSGetExecutablePath(out, &size) == 0 ? 0 : -1;
#else
    ssize_t len = readlink("/proc/self/exe", out, out_size - 1);
    if (len < 0) return -1;
    out[len] = '\0';
    return 0;
#endif
}

void board_reset(void)
{
    char self_path[PATH_MAX];
    if (resolve_self_path(self_path, sizeof(self_path)) != 0) {
        fprintf(stderr, "[BOARD RESET] could not resolve own path, exiting instead\n");
        _exit(1);
    }

    char *argv[] = { self_path, NULL };
    execv(self_path, argv);

    // only reached if execv itself failed
    fprintf(stderr, "[BOARD RESET] execv failed, exiting instead\n");
    _exit(1);
}
