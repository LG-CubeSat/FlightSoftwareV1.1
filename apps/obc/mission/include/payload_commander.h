#ifndef OBC_MISSION_PAYLOAD_COMMANDER_H
#define OBC_MISSION_PAYLOAD_COMMANDER_H

#include <stdint.h>

int payload_commander_take_photo(const char *out_path);

int payload_commander_compress_photo(const char *in_path, const char *out_path);

int payload_commander_downlink_photo(const char *photo_path);

int payload_commander_point_to_sun(void);

int payload_commander_downlink_telemetry_record(uint64_t offset, uint64_t *next_offset, int *end_of_log);

#endif // OBC_MISSION_PAYLOAD_COMMANDER_H
