#ifndef OBC_COMPUTE_SSDV_CODEC_H
#define OBC_COMPUTE_SSDV_CODEC_H

#include <stdint.h>
#include <stddef.h>

#define SSDV_RADIO_PKT_SIZE 128 // e22 max payload is 240

/*
Callsign and image id are for ground station to reassemble images
This is crucial if we are transmitting multiple at once
*/
int ssdv_encode_image(const uint8_t *jpeg, 
    size_t jpeg_len, 
    const char *callsign, 
    uint8_t image_id, 
    uint8_t *out, 
    size_t out_cap, 
    size_t *out_len
);

#endif
