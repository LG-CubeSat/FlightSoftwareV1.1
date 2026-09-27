# LG-CubeSat CCSDS Radio Stack

This module gives flight software and ground software one small, shared way to
compress telemetry and place it into radio packets. It deliberately does not
configure a radio, choose a modulation, or replace SSDV.

## The two standards in use

The project uses one mission profile built from two standards that solve two
different problems:

- **CCSDS 121.0-B-3** reduces a sequence of integer samples without losing any
  information. It is the adaptive extended-Rice compressor.
- **CCSDS 133.0-B-2** adds the six-byte Space Packet header used to identify,
  count, and size packets.

Think of compression as folding a letter and a Space Packet as its addressed
envelope. Neither replaces the other. The exact profile layered on those
standards is specified in [ccsds_mission_profile.md](ccsds_mission_profile.md).

## Downlink and uplink

```text
integer samples -> CCSDS 121 encode --+
                                        +-> LG profile -> Space Packet -> radio
one SSDV packet -> unchanged ----------+

radio bytes -> stream parser -> Space Packet -> CRC check -> command/payload
                                                    +-----> CCSDS 121 decode
```

The packet type bit in the standard Space Packet header distinguishes
telemetry from commands. A second TM/TC frame format is intentionally not
layered around it yet. The E22's exact packet limits and built-in error
handling must be confirmed before that extra overhead can be justified.

## Source layout

| File | Job |
|---|---|
| `shared/ccsds/src/compression.c` | Safe wrapper around pinned libaec v1.1.7 |
| `shared/ccsds/src/space_packet.c` | CCSDS Space Packet header build/parse |
| `shared/ccsds/src/profile.c` | LG-CubeSat profile header, APID rules, and CRC |
| `shared/ccsds/src/stream.c` | Reassemble packets received in arbitrary UART chunks |
| `shared/ccsds/src/crc32.c` | End-to-end CRC-32/ISO-HDLC |
| `tools/ccsds_tool.c` | Host-side demonstration and packet inspector |
| `tests/test_ccsds.c` | Golden header, compression, corruption, SSDV, and stream tests |

Application code should normally include the umbrella header:

```c
#include "ccsds/ccsds.h"
```

## Compressing telemetry

CCSDS 121 operates on integer samples, not arbitrary C structs. The sample
width is the number of meaningful bits in each reading. This example describes
16-bit unsigned readings:

```c
const ccsds_121_config_t config = {
    .bits_per_sample = 16,
    .block_size = 8,
    .reference_sample_interval = 4,
    .signed_samples = false,
    .preprocess = true,
    .restricted_code_options = false
};

size_t compressed_length;
ccsds_status_t status = ccsds_121_encode(
    &config,
    sample_bytes,
    sample_count,
    compressed,
    sizeof(compressed),
    &compressed_length
);
```

All sample bytes are in network byte order: most-significant byte first. A
12-bit sample therefore occupies two bytes. Its four unused high bits must be
zero. For example, signed `-1` is the 12-bit value `0x0FFF`, stored as bytes
`0F FF`. This canonical form makes files identical on ARM, x86, and the ground
computer.

Use `ccsds_121_encoded_bound()` before allocating an output buffer. Compression
can occasionally be larger than the input; the codec automatically has a
no-compression block option, but it still needs room for headers and code IDs.

The decoder must receive the same configuration and the original sample count.
The LG profile carries both values with the compressed bytes.

## Building a packet

After compression, wrap the result:

```c
size_t packet_length;
status = lg_ccsds_profile_build(
    LG_CCSDS_CONTENT_COMPRESSED_TELEMETRY,
    sequence_count,
    sample_count,
    &config,
    compressed,
    compressed_length,
    packet,
    sizeof(packet),
    &packet_length
);
```

The caller owns the 14-bit sequence counter and increments it modulo 16384.
Keeping that state out of the library prevents two telemetry producers from
silently sharing one global counter.

`packet` and `packet_length` are ready to hand to `radio_send()`. This CCSDS
module does not call `radio_send()` itself, which keeps it independently
testable and prevents it from depending on the unfinished real E22 driver.

## Receiving arbitrary chunks

A UART read might return half a packet or several packets. Give the stream
parser a caller-owned buffer and a callback:

```c
uint8_t packet_storage[1024];
ccsds_stream_parser_t parser;

ccsds_stream_parser_init(&parser, packet_storage, sizeof(packet_storage));
ccsds_stream_parser_feed(
    &parser,
    uart_bytes,
    uart_length,
    handle_complete_packet,
    context,
    &packets_delivered
);
```

The callback runs once for each complete Space Packet. It should call
`lg_ccsds_profile_parse()`, which checks the APID/type pairing, profile fields,
length, and CRC before exposing the payload.

The parser uses the Space Packet length field, not a sync marker. If the real
radio can insert or delete bytes rather than merely corrupt or drop a complete
radio packet, the radio transport will also need a synchronization mechanism.
That decision belongs with the exact E22 operating mode.

## SSDV images

SSDV already packetizes a JPEG into protected 256-byte units. Recompressing
those bytes with CCSDS 121 wastes CPU and normally grows the data. Build one
profile packet per SSDV packet instead:

```c
lg_ccsds_profile_build(
    LG_CCSDS_CONTENT_SSDV,
    sequence_count,
    1,
    NULL,
    ssdv_packet,
    LG_CCSDS_SSDV_PACKET_SIZE,
    output,
    output_capacity,
    &output_length
);
```

The parser enforces the 256-byte size and exposes the original SSDV bytes
without modification.

## Errors and safety rules

- Every write API takes a destination capacity.
- Parsing never trusts a length before checking the available bytes.
- There is no library logging and no hidden mutable state.
- The framing/profile code performs no dynamic allocation. libaec allocates
  its internal coding state during an encode or decode call.
- CRC detects accidental corruption; it is **not command authentication**.
  Commands must not be accepted from an operational radio until the mission
  adds an authenticated-command policy.
- A CRC-valid command can still be dangerous. The command dispatcher must
  continue validating command IDs, lengths, allowed states, and ranges.

## Running the checks

```bash
cmake -S . -B build -DHW_MODE=OFF -DBUILD_TESTS=ON
cmake --build build --target ccsds_test ccsds_tool
ctest --test-dir build -R ccsds_test --output-on-failure
./build/bin/ccsds_tool demo
./build/bin/ccsds_tool inspect packet.bin
```

The radio and mission applications are not wired to this module yet. That
integration should happen only after the radio owner confirms whether one call
to `radio_send()` preserves a complete packet and what maximum payload the
selected E22 mode supports.
