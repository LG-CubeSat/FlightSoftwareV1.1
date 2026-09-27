#include "camera.h"
#include <stdio.h>
#include <string.h>
#include <spawn.h>
#include <sys/wait.h>

#define CAMERA_WIDTH 640
#define CAMERA_HEIGHT 480
#define CAMERA_JPEG_QUALITY 50 // tune to get above rez

/* 
'#' stringizes the value.
We use two helpers because a '#' will string the macro not the macro value
So we force it to expand the macro first with a seperate helper
*/
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

extern char **environ;

int camera_capture(const char *out_path)
{
    char *argv[] = {
        "rpicam-jpeg", 
        "-o", (char *)out_path,
        "--width", STR(CAMERA_WIDTH),
        "--height", STR(CAMERA_HEIGHT),
        "--quality", STR(CAMERA_JPEG_QUALITY),
        "-n", // no preview window. Basically its headless
        NULL 
    };

    pid_t pid;
    int rc = posix_spawnp(&pid, "rpicam-jpeg", NULL, NULL, argv, environ);
    if (rc != 0) {
        fprintf(stderr, "[CAMERA] posix_spawnp failed: %s\n", strerror(rc));
        return -1;
    }

    int status;
    // waitpid suspends program until the program (pid) changes state: finnish or crashes
    if (waitpid(pid, &status, 0) != pid) {
        fprintf(stderr, "[CAMERA] waitpid failed\n");
        return -1;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "[CAMERA] rpicam-jpeg exited abnormally (status=%d)\n", status);
        return -1;
    }

    printf("[CAMERA] captured photo -> %s\n", out_path);
    fflush(stdout);
    return 0;
}