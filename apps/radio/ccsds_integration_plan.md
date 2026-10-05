# CCSDS Integration Plan

## Why this doc exists

This is the next step after the real E22 radio driver (`platform/real/drivers/radio.c`):
right now `radio_send()`/`radio_receive()` move raw, unframed bytes. Nothing
on either end knows where one packet stops and the next starts, nothing
checks for corruption, and there's no way to tell a photo packet from a
telemetry reading from a command. CCSDS (Consultative Committee for Space
Data Systems) is the standard answer to that problem, and — importantly —
**most of the work is already built**, just not merged or wired in yet. This
doc explains the standard, documents what already exists, and lays out the
remaining steps to actually put bytes on the radio through it.

## What CCSDS actually is

CCSDS isn't one thing — it's a family of standards international space
agencies use so ground and flight software built by different teams can
still talk to each other. This project only needs two of them:

- **CCSDS 133.0-B-2 (Space Packet Protocol)** — defines a small, fixed
  6-byte header that goes in front of any chunk of data: a version number, a
  flag for telemetry-vs-command, an **APID** (a number identifying *what
  kind* of packet this is — think of it like a port number), a sequence
  count (so a receiver can detect a dropped or reordered packet), and a
  length. It's just an addressed envelope — it says nothing about what's
  inside.
- **CCSDS 121.0-B-3 (Lossless Data Compression)** — an adaptive Rice-coding
  compressor for sequences of fixed-width integer samples (sensor readings,
  telemetry values). It's lossless and has no idea about packets — it just
  shrinks a block of numbers.

Neither one replaces SSDV (already in this repo via `libs/ssdv`, used by
`compute`): SSDV is its own domain-specific JPEG packetization scheme.
Think of it as: SSDV is how a photo gets cut into pieces; CCSDS 121 is how a
list of numbers gets smaller; a CCSDS Space Packet is the envelope either
one rides inside of over the radio.

## What already exists — and where it's hiding

**The useful discovery here**: a nearly-complete implementation of all of
this already exists, built and tested, but sitting on an **unmerged remote
branch**, `origin/CCSDS-Wesley` (not in `main`, not in your working tree).
It is *not* the `libs/CCSDS_121.0` submodule mentioned in `README.md` — that
submodule ships precompiled x86-64 Linux binaries with no source and is a
dead end; `CCSDS-Wesley` instead implements CCSDS 121 compression itself on
top of `libs/libaec` (a real open-source Rice-coding library, vendored as
its own submodule).

What's on that branch, already written and passing its own test suite:

| Path | What it does |
|---|---|
| `shared/ccsds/src/space_packet.c` | Builds/parses the 6-byte CCSDS primary header |
| `shared/ccsds/src/compression.c` | Safe wrapper around libaec for CCSDS 121 encode/decode |
| `shared/ccsds/src/profile.c` | This project's own 16-byte header on top of Space Packet (APID rules, content type, CRC) — see wire format below |
| `shared/ccsds/src/stream.c` | Reassembles complete packets out of arbitrary UART chunk boundaries |
| `shared/ccsds/src/crc32.c` | CRC-32/ISO-HDLC end-to-end integrity check |
| `tools/ccsds_tool.c` | Host-side CLI for building/inspecting a packet by hand |
| `tests/test_ccsds.c` | 6 test groups: header round-trip, compression matrix, corruption, SSDV passthrough, stream reassembly |
| `docs/ccsds_radio_stack.md`, `docs/ccsds_mission_profile.md` | The design docs this plan is built from |

Full umbrella header: `#include "ccsds/ccsds.h"`.

The branch's own documentation is explicit about why it stops where it
does:

> "The radio and mission applications are not wired to this module yet.
> That integration should happen only after the radio owner confirms
> whether one call to `radio_send()` preserves a complete packet and what
> maximum payload the selected E22 mode supports."

That's this plan's job.

## The wire format (already fully specified)

```text
+----------------------+-------------------+----------------+------------+
| CCSDS primary header | LG profile header | payload        | CRC-32     |
| 6 bytes               | 16 bytes          | 0..65516 bytes | 4 bytes    |
+----------------------+-------------------+----------------+------------+
```

APIDs (what distinguishes packet *kinds*):

| APID | Type | Content |
|---:|---|---|
| `0x001` | Telemetry | Raw application telemetry |
| `0x002` | Telemetry | CCSDS 121-compressed integer samples |
| `0x003` | Telemetry | One unchanged 256-byte SSDV packet |
| `0x100` | Command | One mission command payload |

Full byte-level detail (compression flag bits, reserved fields, CRC
parameters) is in `docs/ccsds_mission_profile.md` on the `CCSDS-Wesley`
branch — that doc is the authoritative contract and should just be merged
in, not re-derived here.

## Where this plugs into the current codebase

Traced against what actually exists right now, not guessed:

- **Downlink (photos)**: `apps/obc/mission/src/payload_commander.c`'s
  `payload_commander_downlink_photo()` currently reads the *entire*
  SSDV-packetized file back from `data` over IPC into one buffer
  (`photo_buf`), then calls `radio_send(photo_buf, total)` **once**, as one
  undifferentiated blob. There is no per-packet framing, no CRC, no way for
  a receiver to tell where one 256-byte SSDV packet ends and the next
  begins if any bytes are dropped. This is the main gap CCSDS framing
  closes.
- **Downlink (telemetry)**: nothing currently sends telemetry over radio at
  all — `mission`/`time`/`compute` only exchange internal IPC heartbeats
  and CSP commands on the I2C bus. A telemetry producer for APID `0x001`
  (and eventually `0x002` compressed) doesn't exist yet; it's new work, not
  a rewire.
- **Uplink (commands)**: `radio_receive()` exists and returns raw bytes, but
  nothing currently calls it from the mission/command path, and nothing
  parses received bytes into a Space Packet before handing them to whatever
  dispatches `csp_commands`/`obc_relay_protocol` command envelopes. This is
  also new work.
- **SSDV packet size vs. CCSDS overhead**: `compute`'s `ssdv_codec.c`
  produces fixed `SSDV_PKT_SIZE`-byte (256-byte) packets — which matches
  `LG_CCSDS_SSDV_PACKET_SIZE` in the profile exactly, so no resizing is
  needed there. Each 256-byte SSDV packet becomes one Space Packet of
  `6 + 16 + 256 + 4 = 282` bytes once framed.
- **The one open hardware question that blocks all of this**: `radio.c`'s
  `radio_send()` just loops `write()` until all bytes are flushed — it has
  no concept of a "maximum single packet size" because the E22's real
  transparent-mode packet/payload limit (and whether `radio_send()` must be
  called once per CCSDS packet, or can safely batch several) isn't
  confirmed yet. `platform/real/include/radio_pins.h` already flags the
  GPIO pins and UART device as placeholders pending real board wiring —
  this is the same category of "needs real hardware or datasheet
  confirmation" unknown.

## Phased plan

### Phase 1 — Bring the module in
1. Merge (or rebase, if `main` has moved since it branched)
   `origin/CCSDS-Wesley` into this branch. It touches `.gitmodules`
   (adds `libs/libaec`), `CMakeLists.txt`, `shared/CMakeLists.txt`, adds
   `shared/ccsds/`, `tools/ccsds_tool.c`, `tests/test_ccsds.c`, and its two
   design docs.
2. `git submodule update --init --recursive` to pull in `libs/libaec`.
3. Build and run as-is, unmodified, to confirm it still passes on top of
   whatever `main` has picked up since the branch was cut:
   ```bash
   cmake -S . -B build -DHW_MODE=OFF -DBUILD_TESTS=ON
   cmake --build build --target ccsds_test ccsds_tool
   ctest --test-dir build -R ccsds_test --output-on-failure
   ```
4. Resolve the `apps/obc/data` → `apps/radio/data` move's effect, if any —
   `CCSDS-Wesley` branched before that move, so merge conflicts are
   plausible in `CMakeLists.txt`/`apps/obc/CMakeLists.txt`. Check by hand
   rather than trusting an automatic merge for those two files.

### Phase 2 — Resolve the E22 payload-limit question
This blocks real wiring, per the branch's own docs. Needs one of:
- The E22-400T30D datasheet's stated max transparent-mode payload per
  transmission, confirmed against the module variant actually on hand, or
- An empirical test once real hardware exists: send a known 282-byte
  (framed SSDV packet) buffer through `radio_send()` and confirm the far
  end's `radio_receive()` gets it whole, in one `AUX` busy/idle cycle.

If 282 bytes exceeds the module's single-transmission limit, the options
are (a) the E22's own internal sub-packetization (many LoRa-style modules
fragment transparently), or (b) shrinking `LG_CCSDS_SSDV_PACKET_SIZE`
framing overhead isn't adjustable (profile header is fixed at 16 bytes) so
it would mean fragmenting the *SSDV* packets smaller upstream — a
`compute`-side change, not a `radio`-side one. Don't guess; confirm first.

### Phase 3 — Wire the downlink (photos)
In `payload_commander_downlink_photo()` (`apps/obc/mission/src/payload_commander.c`):
- Replace the single `radio_send(photo_buf, total)` call with a loop over
  `total` in `LG_CCSDS_SSDV_PACKET_SIZE`-byte strides.
- For each 256-byte SSDV packet, call
  `lg_ccsds_profile_build(LG_CCSDS_CONTENT_SSDV, sequence_count++, 1, NULL, packet, 256, framed, sizeof(framed), &framed_len)`,
  then `radio_send(framed, framed_len)`.
- `sequence_count` must be owned by this function (or `mission` generally)
  per-APID, incrementing modulo 16384, per the profile's sequence-count
  rule — don't let it reset to 0 on every photo.
- Check every `lg_ccsds_profile_build` return against `CCSDS_OK` before
  sending; a `CCSDS_ERROR_*` here means a local bug, not a radio problem,
  and shouldn't be silently sent anyway.

### Phase 4 — Wire the uplink (commands)
This is new plumbing, not a rewire:
- Somewhere in the command path (likely `apps/obc/commands`, matching
  where `obc_relay_protocol`/`csp_commands` already live), add a loop that
  calls `radio_receive()`, feeds the bytes into a
  `ccsds_stream_parser_t` (`ccsds_stream_parser_feed`), and on each
  complete packet delivered to the callback, calls
  `lg_ccsds_profile_parse()`.
- Only packets with `content_type == LG_CCSDS_CONTENT_COMMAND` and
  `header.type == CCSDS_PACKET_TYPE_COMMAND` should be forwarded into the
  existing command-dispatch path.
- **Do not treat a CRC-valid command as authenticated or safe** — the
  branch's own docs are explicit that CRC is corruption detection only.
  Command ID/length/state/range validation in the existing dispatcher must
  still happen; CCSDS framing doesn't replace it, and no authenticated-command
  policy exists yet (flagged as a real open gap in `docs/ccsds_radio_stack.md`).

### Phase 5 — Telemetry downlink (new feature, lower priority)
- Decide which subsystem owns periodic telemetry (likely `time` or a new
  small producer in `mission`, since those already run periodic threads).
- Start with APID `0x001` (raw telemetry, `LG_CCSDS_CONTENT_RAW_TELEMETRY`)
  — no compression config needed, simplest path to a first working downlink
  frame.
- Only move to APID `0x002` (CCSDS 121 compressed) once there's an actual
  fixed-width integer sample stream worth compressing (e.g. a sensor
  polling loop), since compression only pays off on sequences of same-width
  samples, not arbitrary structs.

### Phase 6 — Validation
- Extend `tests/test_full_constellation.c` (or add a sibling test) that
  exercises the full path: fake SSDV bytes → framed through
  `lg_ccsds_profile_build` → `ccsds_stream_parser_feed` →
  `lg_ccsds_profile_parse` → bytes-identical to the original, entirely in
  SIM mode (no hardware needed for this part).
- Once real hardware is available, a loopback test (E22 TX wired to a
  second E22 RX, or UART loopback) to validate Phase 2's assumption
  end-to-end.

## Open questions this plan deliberately does not answer yet

- E22 real max single-transmission payload (Phase 2 — blocks Phase 3).
- Who owns the per-APID sequence counters long-term (a single mission-wide
  state, or per-producer) — the branch's docs only say "the caller owns
  it," not where that caller's state should live once there's more than
  one call site.
- Command authentication policy — explicitly called out as unsolved in
  `docs/ccsds_radio_stack.md`, and out of scope for this integration pass.
