#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "mission_health.h"
#include "obc_ipc.h"
#include "radio.h"
#include "payload_commander.h"

#define DEFAULT_OUTPUT_PATH "/tmp/obc_camera_hardware_test.jpg"

static int checks_failed;
static int progress_calls;

static void check_true(const char *description, int condition)
{
    if (condition) {
        printf("[PASS] %s\n", description);
        return;
    }

    fprintf(stderr, "[FAIL] %s\n", description);
    checks_failed++;
}

int IPC_send(
    OBC_Roles_t destination,
    const uint8_t *data,
    uint16_t length
)
{
    (void)destination;
    (void)data;
    (void)length;
    return IPC_ERROR;
}

int IPC_receive_timeout(
    OBC_Roles_t *source,
    uint8_t *buffer,
    uint16_t capacity,
    int timeout_ms
)
{
    (void)source;
    (void)buffer;
    (void)capacity;
    (void)timeout_ms;
}

int radio_send(const uint8_t *data, size_t length)
{
    (void)data;
    (void)length;
    return -1;
}

int radio_receive(uint8_t *buffer, uint16_t max_length)
{
    (void)buffer;
    (void)max_length;
    return -1;
}

void mission_health_payload_progress(void)
{
    progress_calls++;
}

// JPEGs start with FF D8 and end with FF D9
// This function validates that
static int validate_jpeg(const char *path, long *file_size_out)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "[TEST] fopen failed: %s\n", strerror(errno));
        return -1;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return -1;
    }

    long file_size = ftell(file);
    if (file_size < 4) {
        fclose(file);
        return -1;
    }

    unsigned char start[2];
    unsigned char end[2];

    rewind(file);

    if (fread(start, 1, sizeof(start), file) != sizeof(start)) {
        fclose(file);
        return -1;
    }

    if (fseek(file, -2L, SEEK_END) != 0) {
        fclose(file);
        return -1;
    }

    if (fread(end, 1, sizeof(end), file) != sizeof(end)) {
        fclose(file);
        return -1;
    }

    fclose(file);

    printf(
        "[INFO] JPEG markers: start=%02X %02X, end=%02X %02X\n",
        start[0],
        start[1],
        end[0],
        end[1]
    );

    if (start[0] != 0xFFU ||
        start[1] != 0xD8U ||
        end[0] != 0xFFU ||
        end[1] != 0xD9U) {
        return -1;
    }

    if (file_size_out != NULL) {
        *file_size_out = file_size;
    }

    return 0;
}

int main(void)
{
    const char *output_path = getenv("/tmp/obc_camera_hardware_test.jpg");

    if (output_path == NULL || output_path[0] == '\0') {
        output_path = DEFAULT_OUTPUT_PATH;
    }

    /*
    Remove an old image so it cannot make a failed capture look successful
    */
    if (unlink(output_path) != 0 && errno != ENOENT) {
        fprintf(
            stderr,
            "camera_hardware_test: cannot remove old %s\n",
            output_path,
            strerror(errno)
        );
        return 1;
    }

    printf("[TEST] Capturing real image to %s\n", output_path);

    int result = payload_commander_take_photo(output_path);

    check_true(
        "production photo request succeeds",
        result == 0
    );

    check_true(
        "successful capture reports mission progress",
        progress_calls == 1
    );

    struct stat file_status;
    int stat_result = stat(output_path, &file_status);

    check_true(
        "camera created the output file",
        stat_result == 0
    );

    if (stat_result == 0) {
        check_true(
            "camera output is a regular file",
            S_ISREG(file_status.st_mode)
        );

        check_true(
            "camera output is nonempty",
            file_status.st_size > 4
        );
    }

    long jpeg_size = 0;
    int jpeg_result = validate_jpeg(output_path, &jpeg_size);

    check_true(
        "camera output is a complete JPEG",
        jpeg_result == 0
    );

    if (jpeg_result == 0) {
        printf("[INFO] JPEG size: %ld bytes\n", jpeg_size);
    }

    if (checks_failed != 0) {
        fprintf(
            stderr,
            "camera_hardware_test: FAIL (%d checks failed)\n",
            checks_failed
        );
        return 1;
    }

    printf("camera_hardware_test: PASS\n");
    printf("[INFO] Photo retained at %s\n", output_path);
    return- 0;
}