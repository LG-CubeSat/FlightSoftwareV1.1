#include "payload_commander.h"
#include "camera.h"
#include <stdio.h>
#include <stdint.h>
#include "radio.h"

#define MAX_PHOTO_SIZE (64 * 1024) // tune to real photo size

static uint8_t photo_buf[MAX_PHOTO_SIZE];

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

int payload_commander_downlink_photo(const char *photo_path)
{
    printf("[PAYLOAD COMMANDER] Downlinking Photo\n");
    fflush(stdout);

    FILE *f = fopen(photo_path, "rb");
    if (f == NULL) {
        fprintf(stderr, "[PAYLOAD COMMANDER] could not open %s for downlink\n", photo_path);
        return -1;
    }

    size_t bytes_read = fread(photo_buf, 1, sizeof(photo_buf), f);
    fclose(f);

    if (bytes_read == 0) {
        fprintf(stderr, "[PAYLOAD COMMANDER] no data read from %s\n", photo_path);
        return -1;
    }

    // sending it
    if (radio_send(photo_buf, bytes_read) != 0) {
        fprintf(stderr, "[PAYLOAD COMMANDER] downlink failed\n");
        return -1;
    }

    return 0;
}