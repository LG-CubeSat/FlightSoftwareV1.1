#ifndef RADIO_H
#define RADIO_H

#include <stdint.h>
#include <stddef.h>

#define RADIO_MAX_PACKET_SIZE 240 // e22 max bytes per over-the-air packet

/* splits data into <= RADIO_MAX_PACKET_SIZE packets. 0 = all sent, -1 = error */
int radio_send(const uint8_t *data, size_t length);

/* waits for one packet. returns bytes received, 0 = timed out, -1 = error */
int radio_receive(uint8_t *buffer, uint16_t max_length);

#endif
