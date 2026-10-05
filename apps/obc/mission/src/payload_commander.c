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
#include "obc_compute_protocol.h"
#include "ccsds/ccsds.h"

#define MAX_PHOTO_SIZE (64 * 1024) // 64kb, tune to real photo size

static uint8_t photo_buf[MAX_PHOTO_SIZE];

// Per-APID CCSDS sequence count for SSDV telemetry, incremented modulo
// 16384 per docs/ccsds_mission_profile.md. Owned here since this is
// currently the only producer of APID 0x003 packets.
static uint16_t ssdv_sequence_count = 0;

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

int payload_commander_compress_photo(const char *in_path, const char *out_path)
{
    printf("[PAYLOAD COMMANDER] Requesting compression of %s\n", in_path);
    fflush(stdout);

    static uint32_t next_job_id = 1;
    uint32_t job_id = next_job_id++;

    compute_compress_request_t req = {0};
    req.job_id = job_id;
    snprintf(req.in_path, sizeof(req.in_path), "%s", in_path);
    snprintf(req.out_path, sizeof(req.out_path), "%s", out_path);
    req.sample_width = 1; // the mock photo is a raw byte stream, not fixed-width samples

    IPC_send(ROLE_COMPUTE, (const uint8_t *)&req, sizeof(req));

    for (;;) {
        OBC_Roles_t src;
        uint8_t buf[sizeof(compute_result_t)];
        int len = IPC_receive(&src, buf, sizeof(buf));
        if (len != sizeof(compute_result_t)) continue;

        compute_result_t result;
        memcpy(&result, buf, sizeof(result));
        if (result.job_id != job_id) continue; // stale reply from an earlier job, not ours

        if (result.status != COMPUTE_STATUS_OK) {
            fprintf(stderr, "[PAYLOAD COMMANDER] compression of %s failed (status=%d)\n", in_path, result.status);
            return -1;
        }

        printf("[PAYLOAD COMMANDER] Compression done: %u bytes\n", result.output_size);
        fflush(stdout);
        return 0;
    }
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

    if (total % LG_CCSDS_SSDV_PACKET_SIZE != 0) {
        fprintf(stderr, "[PAYLOAD COMMANDER] %s is %zu bytes, not a multiple of the %u-byte SSDV packet size\n",
                photo_path, total, LG_CCSDS_SSDV_PACKET_SIZE);
        return -1;
    }

    // Frame each 256-byte SSDV packet as its own CCSDS Space Packet (APID
    // 0x003, see docs/ccsds_mission_profile.md) before handing it to the
    // radio, instead of sending the whole file as one undifferentiated
    // blob -- see apps/radio/ccsds_integration_plan.md phase 3. One
    // radio_send() call per framed packet; whether the E22's selected mode
    // can carry one of these in a single transmission is still an open
    // question tracked in that doc (phase 2).
    uint8_t framed[CCSDS_SPACE_PACKET_PRIMARY_HEADER_SIZE + LG_CCSDS_PROFILE_HEADER_SIZE +
                   LG_CCSDS_SSDV_PACKET_SIZE + LG_CCSDS_PROFILE_CRC_SIZE];
    for (size_t offset = 0; offset < total; offset += LG_CCSDS_SSDV_PACKET_SIZE) {
        size_t framed_length;
        ccsds_status_t status = lg_ccsds_profile_build(
            LG_CCSDS_CONTENT_SSDV,
            ssdv_sequence_count,
            1,
            NULL,
            photo_buf + offset,
            LG_CCSDS_SSDV_PACKET_SIZE,
            framed,
            sizeof(framed),
            &framed_length);
        if (status != CCSDS_OK) {
            fprintf(stderr, "[PAYLOAD COMMANDER] CCSDS framing failed for %s at offset %zu: %s\n",
                    photo_path, offset, ccsds_status_string(status));
            return -1;
        }
        ssdv_sequence_count = (ssdv_sequence_count + 1) % (CCSDS_SPACE_PACKET_MAX_SEQUENCE_COUNT + 1);

        if (radio_send(framed, framed_length) < 0) {
            fprintf(stderr, "[PAYLOAD COMMANDER] downlink failed at offset %zu.\n", offset);
            return -1;
        }
    }

    printf("[PAYLOAD COMMANDER] downlinked %zu bytes as %zu CCSDS/SSDV packets\n",
           total, total / LG_CCSDS_SSDV_PACKET_SIZE);
    fflush(stdout);

    return 0;
}

int payload_commander_point_to_sun(void)
{
    command_envelope_t cmd = { .command_id = CMD_POINT_TO_SUN, .seq = 1 };
    relay_request_t req = { .dest_addr = ADCS_ADDRESS, .dest_port = ADCS_CMD_PORT, .length = sizeof(cmd) };
    memcpy(req.payload, &cmd, sizeof(cmd));
    return IPC_send(ROLE_COMMANDS, (const uint8_t *)&req, sizeof(req)) < 0 ? -1 : 0;
}