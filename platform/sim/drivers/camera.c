#include "camera.h"
#include <stdio.h>

int camera_capture(const char *out_path)
{
    FILE *f = fopen(out_path, "w");
    if (f == NULL) {
        fprintf(stderr, "[CAMERA] failed to open %s for writing", out_path);
        return -1;
    }
    fprintf(f, "MOCK PHOTO DATA\n");
    fclose(f);

    printf("[CAMERA] (mock) captured photo -> %s\n", out_path);
    fflush(stdout);
    return 0;
}