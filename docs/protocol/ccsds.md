# CCSDS Space Packet Protocol

## Overview
We utilize the **CCSDS (Consultative Committee for Space Data Systems)** standard for all inter-processor and ground-to-space communication. This provides a standardized binary format that is platform-independent.

## The Primary Header (6 Bytes)
Every packet begins with a fixed 6-byte header:

1.  **Packet ID (2 Bytes):**
    *   Version Number (3 bits)
    *   Type (1 bit: 0=TLM, 1=CMD)
    *   Secondary Header Flag (1 bit)
    *   **APID** (11 bits): Application Process ID. This acts like a "Port Number" to route the packet to the correct subsystem (e.g., ADCS, EPS).
2.  **Sequence Control (2 Bytes):** A counter that increments with every packet sent to detect dropped data.
3.  **Length (2 Bytes):** Total payload bytes **minus 1**.

## APID Catalog
Our current routing table:
- `0x01`: OBC Command
- `0x02`: ADCS Command
- `0x03`: EPS Command
- `0x12`: ADCS Telemetry
- `0x13`: EPS Telemetry

## Endianness and Bit-Ordering
Space standards are **Big Endian** (Network Byte Order). Since our CPUs (ARM/Mac) are **Little Endian**, we use the `ccsds_swap16` helper function to flip bytes before interpreting header fields like the APID.
