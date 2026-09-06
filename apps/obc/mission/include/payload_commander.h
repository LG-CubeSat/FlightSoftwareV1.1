#ifndef OBC_MISSION_PAYLOAD_COMMANDER_H
#define OBC_MISSION_PAYLOAD_COMMANDER_H

int payload_commander_take_photo(const char *out_path);

int payload_commander_downlink_photo(const char *photo_path);

int payload_commander_point_to_sun(void);

#endif // OBC_MISSION_PAYLOAD_COMMANDER_H
