#include "payload_commander.h"
#include "camera.h"
#include <stdio.h>

int payload_commander_take_photo(const char *out_path)
{
    printf("[PAYLOAD COMMANDER] Requesting photo capture\n");
    fflush(stdout);
    if (camera_capture(out_path) != 0) {
       fprintf(stderr, "[PAYLOAD COMMANDER] photo capture failed\n");
        return -1;
    }
    return 0;
}