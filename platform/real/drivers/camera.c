#include "camera.h"
#include <stdio.h>

int camera_capture(const char *out_path)
{
    (void)out_path;
    fprintf(stderr, "[CAMERA] real USB-C capture not implemented yet\n"); // TODO: write camera code
    return -1;
}