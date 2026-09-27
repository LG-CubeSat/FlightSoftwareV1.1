# LG-CubeSat CCSDS Profile v1

This is the exact byte contract shared by the satellite and ground station.
Multi-byte integers are unsigned and most-significant byte first.

## Complete packet

```text
+----------------------+-------------------+----------------+------------+
| CCSDS primary header | LG profile header | payload        | CRC-32     |
| 6 bytes              | 16 bytes          | 0..65516 bytes | 4 bytes    |
+----------------------+-------------------+----------------+------------+
```

The CCSDS Packet Data Length field contains the number of bytes after the
six-byte primary header minus one. This unusual “minus one” rule is part of
CCSDS 133.0-B-2.

## CCSDS primary header

| Bits | Field | Profile rule |
|---:|---|---|
| 3 | Packet Version Number | `000` |
| 1 | Packet Type | `0` telemetry, `1` command |
| 1 | Secondary Header Flag | `1` because the LG header is present |
| 11 | APID | From the table below |
| 2 | Sequence Flags | `11`, one unsegmented packet |
| 14 | Sequence Count | Increment modulo 16384 per APID |
| 16 | Packet Data Length | Data field byte count minus one |

## APID assignments

| APID | Packet type | Content |
|---:|---:|---|
| `0x001` | Telemetry | Raw application telemetry |
| `0x002` | Telemetry | CCSDS 121.0-B-3 compressed integer samples |
| `0x003` | Telemetry | One unchanged 256-byte SSDV packet |
| `0x100` | Command | One mission command payload |

These are mission-managed values. Changing one requires a new profile version
or a coordinated flight-and-ground update.

## LG profile header

| Byte offset | Size | Field | Meaning |
|---:|---:|---|---|
| 0 | 1 | Profile version | `1` |
| 1 | 1 | Content type | `0` raw telemetry, `1` compressed telemetry, `2` SSDV, `3` command |
| 2 | 1 | Compression flags | See below; zero unless content type is compressed telemetry |
| 3 | 1 | Bits per sample | CCSDS 121 `n`; zero unless compressed |
| 4 | 1 | Block size | CCSDS 121 `J`: 8, 16, 32, or 64; zero unless compressed |
| 5 | 2 | Reference interval | CCSDS 121 `r`, measured in blocks; zero unless compressed |
| 7 | 1 | Reserved | Must be zero |
| 8 | 4 | Item count | Original sample count, SSDV packet count, or application record count |
| 12 | 4 | Payload length | Exact number of following payload bytes, excluding CRC |

Compression flag bits:

| Bit | Mask | Meaning |
|---:|---:|---|
| 0 | `0x01` | Signed samples |
| 1 | `0x02` | Unit-delay preprocessing enabled |
| 2 | `0x04` | Restricted code-option set; legal only for 1–4-bit samples |
| 3–7 | — | Reserved; must be zero |

For compressed telemetry, `item count` is the original number of samples. It
is required because the final CCSDS 121 block may contain zero padding.

## Payload rules

### Raw telemetry

The application owns the payload schema. Compression fields are zero.
`item count` describes application records and may be zero if the application schema
does not use it.

### Compressed telemetry

The payload is a CCSDS 121.0-B-3 coded stream produced with the parameters in
the profile header. `item count` must be nonzero.

Samples are stored most-significant byte first before compression:

| Sample bits | Storage bytes |
|---:|---:|
| 1–8 | 1 |
| 9–16 | 2 |
| 17–24 | 3 |
| 25–32 | 4 |

Unused high bits are zero, including for signed samples. Signed samples use
an `n`-bit two's-complement representation in the low `n` bits.

### SSDV

The payload is exactly one 256-byte SSDV packet and `item count` is one. The
bytes are not passed through CCSDS 121.

### Command

The payload belongs to the command application. Compression fields are zero.
CRC validation is necessary but not sufficient: command authentication and
application range/state checks are separate requirements.

## CRC

The final four bytes contain CRC-32/ISO-HDLC in network byte order:

- polynomial: `0x04C11DB7`
- reflected input and output
- initial value: `0xFFFFFFFF`
- final XOR: `0xFFFFFFFF`
- check value for ASCII `123456789`: `0xCBF43926`

CRC coverage begins at byte zero of the CCSDS primary header and ends with the
last payload byte. The CRC field itself is excluded. Protecting the primary
header ensures that corruption of the APID, direction, sequence count, or
length is detected by the profile parser.

## Receiver validation order

1. Collect six primary-header bytes.
2. Calculate the complete packet size and enforce the local buffer limit.
3. Collect exactly that many bytes.
4. Require profile version 1 and an unsegmented packet.
5. Verify CRC before using any payload.
6. Verify packet type, APID, and content type agree.
7. Verify reserved fields, payload length, and compression configuration.
8. Dispatch or decompress only after every check passes.

This order keeps malformed radio input away from application and decompression
code.
