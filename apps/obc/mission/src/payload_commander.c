#include "payload_commander.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "csp_commands.h"
#include "camera.h"
#include "radio.h"
#include "obc_ipc.h"
#include "obc_relay_protocol.h"
#include "obc_data_protocol.h"

#define MAX_PHOTO_SIZE (64 * 1024) // 64kb, tune to real photo size

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

    data_read_request_t req = {0};
    snprintf(req.path, sizeof(req.path), "%s", photo_path);
    IPC_send(ROLE_DATA, (const uint8_t *)&req, sizeof(req));

    size_t total = 0;

    for (;;) {
        OBC_Roles_t src;
        uint8_t buf[sizeof(data_read_reply_t)];
        int len = IPC_receive(&src, buf, sizeof(buf));
        if (len != sizeof(data_read_reply_t)) continue;

        data_read_reply_t reply;
        memcpy(&reply, buf, sizeof(reply));

        if (reply.status != 0) {
            fprintf(stderr, "[PAYLOAD COMMANDER] data reported an error reading %s\n", photo_path);
            return -1;
        }

        if (total + reply.length > sizeof(photo_buf)) {
            fprintf(stderr, "[PAYLOAD COMMANDER] photo too large for photo_buf\n");
            return -1;
        }

        memcpy(photo_buf + total, reply.payload, reply.length);
        total += reply.length;

        if (reply.is_last) break;
    }

    if (total == 0) {
        fprintf(stderr, "[PAYLOAD COMMANDER] no data read from %s\n", photo_path);
        return -1;
    }

    // sending it
    if (radio_send(photo_buf, total) != 0) {
        fprintf(stderr, "[PAYLOAD COMMANDER] downlink failed.\n");
        return -1;
    }

    return 0;
}

int payload_commander_point_to_sun(void)
{
    command_envelope_t cmd = { .command_id = CMD_POINT_TO_SUN, .seq = 1 };
    relay_request_t req = { .dest_addr = ADCS_ADDRESS, .dest_port = ADCS_CMD_PORT, .length = sizeof(cmd) };
    memcpy(req.payload, &cmd, sizeof(cmd));
    return IPC_send(ROLE_COMMANDS, (const uint8_t *)&req, sizeof(req)) < 0 ? -1 : 0;
}