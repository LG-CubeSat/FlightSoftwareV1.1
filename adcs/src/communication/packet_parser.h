#ifndef PACKET_PARSER_H
#define PACKET_PARSER_H

#include <stdint.h>

void packet_parser_initialize(void);

void packet_parser_process(
    const uint8_t *data,
    uint16_t length
);

#endif