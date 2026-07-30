#ifndef CCSDS_H
#define CCSDS_H

#include "stdint.h"

#define MAX_PAYLOAD_SIZE 256

/*
CCSDS (Consulatative Committee for Space Data Systems)
This is a standard so that any system can communicate and get interpreted the same way
// if not packed then compiler may add padding bytes to align with memory bus
*/

// this is merely a header, not the message
typedef struct {
    uint16_t id;
    uint16_t sequence_control;
    uint16_t length;
} __attribute__((packed)) CCSDS_PrimaryHeader;

typedef struct {
    CCSDS_PrimaryHeader header;
    uint8_t payload[MAX_PAYLOAD_SIZE];
    uint16_t crc;
} __attribute__((packed)) CCSDS_Packet_t;

typedef enum {
    APID_OBC_CMD = 0x01, // commands for main computer
    APID_ADCS_CMD = 0x02, // commands for adcs
    APID_EPS_CMD = 0x03, // commands for power
    APID_ADCS_TLM = 0x12, // Telemetry from ADCS
    APID_EPS_TLM = 0x13, // Telemetry from Power
} CCSDS_APID_t;

static inline uint16_t ccsds_swap16(uint16_t val)
{
    return (val << 8) | (val >> 8); // bring second half first, and first half second
}

static inline uint16_t ccsds_get_apid(uint16_t id)
{
    /* 
    extract the last 11 bits of id
    that is the APID (application process ID)
    this lets us determine where to route the packet
     */
    return ccsds_swap16(id) & (0x07FF);
}

#endif

