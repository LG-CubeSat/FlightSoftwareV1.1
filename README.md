# Flight Software V1

Flight software stack for LG-CubeSat, currently aimed at a **November balloon launch** —
the near-term goal is proving the CSP/I2C bus architecture end-to-end on real hardware
before it carries the full satellite. See `docs/roadmap.md` (full satellite plan) and
`docs/balloon_launch_plan.md` (November-specific scope and cut list) for the complete
picture; this file is the practical "clone it, build it, run it" reference.

## Status snapshot

| Piece | State |
|---|---|
| ADCS | **Done** — reference implementation. FreeRTOS task set, command handling, telemetry, full CSP round-trip with OBC. Also self-monitors now: an independent watchdog thread and an out-of-bounds check can trigger a real local reset, and repeated resets can lead to an OBC-directed shutdown — see "OBC internal architecture" below. |
| OBC | No longer a single binary — split into 7 cooperating Linux processes (`supervisor`, `fdir`, `commands`, `compute`, `data`, `mission`, `time`) talking over local IPC, see `apps/obc/roles.md`. **All 7 are real now.** `mission` runs a one-shot scripted balloon timeline (ascent → photo → compress → downlink, against mock camera/radio) plus a recurring `autonomy` thread that periodically commands other subsystems (e.g. telling ADCS to point at the sun). `time` periodically pushes a `CMD_TIME_SYNC` to every known board and can also answer an on-demand sync request. `data` owns all filesystem access — `mission` no longer touches files directly; it asks `data` to stream them back over IPC instead. `compute` runs a real CCSDS-121-style adaptive Golomb-Rice compressor asynchronously, with cancellation — see "OBC internal architecture" below. |
| Comms bus (I2C) | Shared-bus simulation with address-based framing (see below) — multiple nodes on one simulated bus, each filtering to its own traffic. Real I2C HAL backend is still a stub (see Known gaps). |
| EPS / Thermals / Comms (radio HW) | Not yet scaffolded as CSP boards. Camera/radio *interfaces* exist as mock-only contracts for `mission` — see "OBC internal architecture" below; the real E22 radio driver is being built separately by a teammate. |
| FPGA compression / Akida1500 | Out of scope for November; tracked in `docs/roadmap.md` Phase 4. |

## Architecture at a glance

Every board is its own CSP node on a shared I2C bus (SPI and CAN were retired — see
`docs/roadmap.md`'s "Architecture Decisions" section for why):

| Node | CSP Addr | Cmd Port | Telem Port | Status |
|---|---|---|---|---|
| OBC | 1 | — | — | done (SIM) |
| ADCS | 2 | 10 | 20 | **done** |
| EPS | 3 | 11 | 21 | reserved, not built |
| THERMALS | 4 | 12 | 22 | reserved, not built |
| CAMERA | 5 | 13 | 23 | reserved, not built |
| COMMS | 6 | 14 | 24 | reserved, not built |

**The bus contract** (`shared/interfaces/comms_bus.h`) is medium-agnostic: `initialize`,
`send`, and `receive` don't change whether the backend is a Unix-socket simulation or real
I2C hardware. Every message is framed as `[dest_addr][src_addr][length][payload]`
(`shared/interfaces/frame.c`) — the SIM backend (`platform/sim/drivers/comms_i2c.c`)
broadcasts every frame to every connected node and lets each node's `receive()` discard
anything not addressed to it, mirroring how a real shared I2C wire works electrically.
Full design writeup: `docs/i2c_sim_transport_plan.md`.

### OBC internal architecture

The OBC itself is 7 separate Linux processes, not one binary — see `apps/obc/roles.md`
for the full breakdown of who owns what. The ones worth knowing about here:

- **`supervisor`** starts the other 6 processes (`posix_spawn`, resolving each sibling's
  path relative to its own so it works regardless of launch directory), sweeps them for
  liveness, and restarts a crashed or unresponsive one — mechanism only, no policy.
- **`fdir`** is where fault policy lives. Its `health_monitor` receives fire-and-forget
  reset notices from external boards (a board decides and acts on a local fault entirely
  on its own — see below — then tells the OBC after the fact) and tracks how often each
  board has reset; past a threshold, it decides the board needs a full shutdown rather
  than another automatic restart, and asks `commands` to relay that over CSP.

**Board-side self-reset/shutdown** (currently implemented for ADCS) follows the same
sim/real split as the comms bus: `shared/interfaces/board_reset.h` and `board_shutdown.h`
each have a `platform/sim/drivers/` implementation (a Linux process re-exec / clean exit)
and a `platform/real/drivers/` implementation (an ARM Cortex-M `AIRCR` system reset /
`WFI` low-power halt — architectural instructions, not vendor-specific, so no STM32
HAL/CMSIS dependency is needed). A board's own watchdog and bounds-checking call these
directly; the OBC never has to reach in and force anything on a board that's still
responsive.

- **`mission`** owns everything payload/mission-related, split into three pieces that
  mirror the roles.md description of the role:
  - `scheduler` runs a one-shot, linear balloon-flight timeline (a small state machine:
    wait for ascent → take photo → compress → downlink → done) — deliberately scripted,
    no real autonomy, since the balloon flight doesn't need or want decision-making.
  - `payload_commander` is what the scheduler (and autonomy) call to actually do
    something: capture a photo (`camera_capture`), ask `compute` to compress a file
    and block for the result (`payload_commander_compress_photo`), downlink a file
    over the radio (`radio_send`), or command another board (e.g.
    `payload_commander_point_to_sun` builds a `command_envelope_t` and hands it to
    `commands`' relay over `obc_relay_protocol.h`).
  - `autonomy` is a small recurring engine — a table of `{name, interval, last_fired,
    function pointer}` entries, checked once a second — built now even though the
    balloon flight doesn't use real autonomy, because the table-driven shape scales
    cleanly to the real satellite mission and gives other subsystem teams something
    concrete to mock against today. Its only entry right now periodically tells ADCS
    to point at the sun; adding a new periodic behavior is one line in the table, not
    new logic.

  Camera and radio are USB/UART peripherals wired directly to the OBC's Linux box, not
  separate CSP boards — `shared/interfaces/camera.h`/`radio.h` are mock-only contracts
  for now (`platform/sim/drivers/` writes/logs a placeholder; `platform/real/drivers/`
  has an honest not-yet-implemented stub for the camera). **The real radio driver is
  intentionally not touched here** — a teammate is building the real E22 driver
  separately, so `platform/real/drivers/radio.c` doesn't exist and isn't referenced from
  `platform/CMakeLists.txt`'s `HW_MODE` branch, to avoid colliding with that work.

- **`time`** keeps the boards' clocks aligned to the OBC's. Rather than a new CSP port, it
  reuses the same command-envelope/ACK mechanism every board's `command_handler.c` already
  implements: a `CMD_TIME_SYNC` command carrying a `time_sync_command_t` (envelope +
  `unix_time_sec`), sent to each board's own existing command port via `commands`' relay.
  A local `known_boards[]` table (address + cmd port) drives two threads: one broadcasts
  to every board on a timer (`time_sync_broadcast_thread`), the other answers an on-demand
  resync request from a specific board immediately instead of making it wait for the next
  tick (`time_sync_request_thread`, fed by one more row in `commands`' `ingest_thread`
  routing table — a board pushes a self-identifying `time_sync_request_t` to
  `TIME_SYNC_REQUEST_PORT`, same self-identifying-payload idea as `board_reset_notice_t`).
  On-demand queries from *other internal OBC roles* were deliberately left out of scope —
  every internal process already has `clock_gettime()` locally, so there's no real problem
  an IPC round-trip to `time` would solve. `unix_time_sec` comes straight from the OBC
  Linux box's own system clock for now — no real RTC/GPS discipline yet, same "sim now,
  real later" honesty as `camera`/`radio`, and boards don't actually apply the synced time
  anywhere yet (`command_handler.c`'s `CMD_TIME_SYNC` case is an ACK+log placeholder, like
  `CMD_POINT_TO_SUN`).

- **`data`** owns filesystem access, per `roles.md`'s "every read/write goes through
  `data`, not another process touching the filesystem directly" — closing a real
  violation where `mission`'s `payload_commander.c` used to `fopen`/`fread` the photo file
  itself. Split into `filesystem.c` (the only file in the whole codebase that still calls
  `fopen`) and `storage.c` (the IPC-facing thread that dispatches by message size to the
  read or write path). Files cross IPC chunked, not as one message: `obc_ipc.c` caps a
  single message at 256 bytes, so a 64KB photo can't fit in one `data_read_reply_t` —
  `filesystem_stream_file` streams `DATA_CHUNK_SIZE`-sized (~200 byte) chunks with
  `{status, offset, length, is_last}` until `is_last`, and the not-found case sends one
  `status=-1, is_last=1` reply so the caller's loop always terminates instead of hanging.
  The write path (`filesystem_write_chunk`) is the same idea in reverse — `data_write_chunk_t`
  carries its own `path` (writes are self-contained per-message, no session state kept in
  `data` across chunks; `offset==0` truncates/creates, `offset>0` appends) — with one ack
  per chunk so the writer can abort immediately on the first failure. `filesystem_stream_file`
  also retries a transient `IPC_send` failure a bounded number of times before giving up: a
  real bug caught by testing had a fast reader outrun `obc_ipc`'s connection backlog,
  silently dropping chunks (`IPC_send` is deliberately fail-fast and doesn't retry itself —
  see Known gaps) — including, once, the final chunk, which left the reader waiting forever
  for one that would never arrive.
- **`compute`** is the last of the 7 to go from stub to real, and the first genuinely
  asynchronous one: `threading.md` specs it as "a dispatcher blocks on ipc for work
  requests; a separate worker thread does the actual compute... so a long job never blocks
  the dispatcher from accepting a new request or a cancellation," and `compute.c` follows
  that directly. A single job slot (one photo per flight is all that's needed — a second
  `compress` request while one is running gets an immediate `COMPUTE_STATUS_BUSY`, not
  queued); the dispatcher hands each job to a detached worker thread, which reads the input
  from `data`, compresses it, writes the result back to `data`, and checks a cancellation
  flag between chunks throughout. Because `obc_ipc` only supports one blocking receiver per
  process, the dispatcher stays the *only* thread that ever calls `IPC_receive` — anything
  it gets from `ROLE_DATA` is routed to the worker through a small single-slot mailbox
  (mutex + condition variable) instead of the worker calling `IPC_receive` itself, which
  would race the dispatcher's own `accept()`.

  The compression itself (`rice_codec.c`) is adaptive block Golomb-Rice coding with a
  zero-block (RLE) and verbatim fallback per 16-sample block — the same core scheme CCSDS
  121.0 ("Lossless Data Compression") uses for real spacecraft telemetry, scoped down to
  the options that matter here. It's parameterized by *sample width* (1/2/4 bytes), not
  hardcoded to bytes, so the same codec correctly compresses a raw byte stream (today's
  mock photo) or a fixed-width telemetry/sensor sample stream — CCSDS 121.0 is itself a
  general-purpose sample-stream compressor, not an image-specific one (true image-aware
  compression, CCSDS 122.0, adds a 2D wavelet transform in front of an entropy coder like
  this one — explicitly out of scope here). A from-scratch `bitstream.c` (bit-level
  writer/reader) underpins it, since Rice codes aren't byte-aligned. Two real bugs were
  caught by round-trip testing here, not review: an unmasked delta computation that only
  wrapped correctly at 32 bits regardless of the actual sample width (corrupted width 1/2
  data silently), and undefined behavior in a signed left-shift reading a width-4 sample's
  top byte when its value was ≥ 128.

Every long-lived OBC role sends a periodic no-payload heartbeat ping to `supervisor`
(`IPC_send(ROLE_SUPERVISOR, NULL, 0)`, see any role's `heartbeat.c`) — without it,
`supervisor`'s frozen-check can't distinguish "quietly working" from "hung," and will
restart a perfectly healthy process. All 6 non-supervisor roles do this today.

## Repository layout

```
apps/
├── adcs/                  # FreeRTOS MCU app -- reference implementation, now with local fault handling
│   └── src/manager/fault_manager.c   # Watchdog thread, bounds checking, reset/shutdown triggers
└── obc/                   # 7 separate Linux processes, not one binary -- see apps/obc/roles.md
    ├── supervisor/         # Process lifecycle: spawns, reaps, restarts the other 6
    ├── fdir/               # Fault policy: health_monitor tracks board resets, fallback acts on them
    ├── commands/           # Owns the external bus: ingest (inbound routing), relay (outbound), heartbeat
    ├── mission/            # scheduler (balloon timeline), payload_commander, autonomy, heartbeat
    ├── time/               # time_sync: periodic + on-demand CMD_TIME_SYNC to every board, heartbeat
    ├── data/               # storage (IPC service) + filesystem (the only fopen left), heartbeat
    ├── compute/            # dispatch (async job engine) + rice_codec + bitstream, heartbeat
    └── ipc/                # Internal IPC shared by all 7

shared/
├── interfaces/
│   ├── comms_bus.h         # The medium-agnostic bus contract
│   ├── board_reset.h       # Board self-reset, sim (re-exec) vs real (Cortex-M AIRCR) impl in platform/
│   ├── board_shutdown.h    # Board halt, sim (exit) vs real (Cortex-M WFI) impl in platform/
│   ├── camera.h            # Photo capture, mock only for now (mission's payload_commander)
│   ├── radio.h             # Ground downlink, mock only -- real E22 driver owned separately, see above
│   ├── frame.h / frame.c   # Wire framing (addressing + serialization), shared by every backend
├── csp/                    # CSP-to-transport glue (csp_network.c, csp_if_spi.c, csp_commands.h)

platform/
├── sim/drivers/    # comms_i2c.c, board_reset.c, board_shutdown.c, camera.c, radio.c -- build against today
└── real/drivers/   # Hardware-backed; comms_i2c.c is stale (see Known gaps), radio.c intentionally absent

tests/                      # CTest-registered integration tests, see Testing below
docs/                       # roadmap.md, balloon_launch_plan.md, api_contracts.md, etc.
libs/libcsp/                # CSP protocol implementation (git submodule)
rtos/                       # FreeRTOS kernel + POSIX/hardware ports
```

Full convention (naming, where new subsystems go, CMake patterns): `docs/directory_conventions.md`.

## Getting started

### Clone (this repo uses a git submodule for libcsp)

```bash
git clone --recurse-submodules https://github.com/LG-CubeSat/FlightSoftwareV1.git
# already cloned without --recurse-submodules? run:
git submodule update --init --recursive
```

### Prerequisites

- CMake 3.16+
- A C11 compiler (GCC or Clang)
- pthreads (standard on Linux/macOS)

### Build (simulation mode — the default)

```bash
cmake -S . -B build -DHW_MODE=OFF
cmake --build build
```

### Run it

Two terminals — OBC first. Running `obc_supervisor` alone brings up the whole OBC: it
spawns the other 6 real roles internally (`commands` is the bus master and needs to be
listening before ADCS connects, though ADCS retries if it isn't yet):

```bash
# terminal 1
./build/bin/obc_supervisor

# terminal 2
./build/apps/adcs/adcs_sim
```

With both sides up: `mission` runs its scripted balloon timeline on its own (wait for
"ascent" → mock photo capture → `compute` compresses it → `data` streams the compressed
result back → mock radio downlink, logged at each step — no ADCS involvement); its
`autonomy` thread periodically sends ADCS a `CMD_POINT_TO_SUN` through `commands`'
`relay`; and `time` immediately (then periodically) sends ADCS a `CMD_TIME_SYNC` the same
way — watch for `[COMMAND HANDLER] Time sync command received.` in ADCS's log, and
`[COMPUTE] job 1: done (... -> ... bytes)` for the compression step. To exercise the fault
path instead, send ADCS a command built the same way
`fallback.c` does (a `relay_request_t` over `IPC_send(ROLE_COMMANDS, ...)`) — an
out-of-range `CMD_MOVE_TO_POSITION` will walk the whole chain end to end: ADCS detects
the fault, resets itself, notifies the OBC, and after enough repeats `fdir` shuts the
board down.

### Build for hardware

```bash
cmake -S . -B build -DHW_MODE=ON
cmake --build build
```

## Testing

```bash
cmake -S . -B build -DHW_MODE=OFF -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

`BUILD_TESTS` defaults to `ON`, but if your `build/` directory was configured before (a
cached `OFF` sticks around across plain re-runs of `cmake -S . -B build`), pass
`-DBUILD_TESTS=ON` explicitly once to pick it back up.

All seven tests build and pass. `position_command_test` and `command_ack_test` *are* the
OBC's CSP node themselves (the same `csp_network_init(OBC_ADDRESS, 1)` call any real OBC
role makes), talking to a real spawned `adcs_sim` — this tests ADCS's actual command/task
pipeline over the real wire contract without depending on which internal OBC processes
happen to exist. `full_constellation_test` is the one exception: it spawns the *real*
`obc_supervisor`, which brings up the real 7-process constellation exactly like a real
launch would, alongside a real `adcs_sim`. `rice_codec_test` is the one exception in the
other direction: no processes, no IPC at all — just direct calls into `compute`'s codec:

| Test | What it proves |
|---|---|
| `comms_bus_test` | Two nodes (OBC + ADCS), addressed frames flow correctly both directions over the real (non-mocked) transport. |
| `comms_bus_addressing_test` | Three nodes (OBC + two slaves) — a message addressed to one slave is *not* delivered to the other. This is the one that actually exercises the broadcast-and-filter design; two-node tests can't catch a misrouted message since there's nowhere else for it to go. |
| `position_command_test` | Full-stack happy path: repeated `CMD_MOVE_TO_POSITION` commands get ACKed, activate every task the command should (Control, Estimation, Sensor, Telemetry — not Housekeeping), and get a matching telemetry report back, all repeated across multiple cycles so a "works once then hangs" regression can't slip through. |
| `command_ack_test` | ACK/NACK protocol edge cases `position_command_test` doesn't cover: an unrecognized command_id gets NACKed, an undersized `CMD_MOVE_TO_POSITION` (missing its target) gets NACKed, and a valid command right after both still gets ACKed — proving bad input doesn't wedge the handler. |
| `full_constellation_test` | The only test that checks the processes actually work *together*, not just individually: every real role comes up, `autonomy`'s sun-pointing and `time`'s sync both reach ADCS, `mission`'s full scripted timeline runs end to end (ascent → photo → compress → `data` streams it back → downlink), and `supervisor` never falsely restarts a healthy process. `MISSION_ASCENT_WAIT_SEC` and `TIME_SYNC_INTERVAL_SEC` env vars let it run the real ~90 min / 5 min timers in seconds — production defaults are untouched unless the var is set. |
| `rice_codec_test` | Round-trips `rice_compress`/`rice_decompress` across sample widths and data shapes (all-zero, constant, ramp, random/incompressible, odd-length, tail bytes, high byte values, empty) — the cheapest place to catch a bit-packing or delta/zig-zag bug, and where the two real bugs described in "OBC internal architecture" above were actually found. |
| `compute_async_test` | The two behaviors that make `compute` different from every other request/reply role: a second concurrent compress request gets `COMPUTE_STATUS_BUSY` (not queued) while one is running, and an in-flight job can be cancelled mid-run. Forks two real requester processes with different roles against real `obc_data`/`obc_compute` binaries — `COMPUTE_CHUNK_DELAY_MS` widens the job's runtime so both checks land reliably. |

Building `full_constellation_test` caught a real bug: `autonomy_thread` marked an action
as "done" (updating `last_fired`) even when its `IPC_send` failed — so a transient startup
race (firing before `commands` was listening yet) silenced retries for a full 10 minute
interval instead of retrying on the very next tick. Fixed by only updating `last_fired` on
success (`apps/obc/mission/src/autonomy.c`). Caught by running the new test back-to-back
~15 times and seeing it fail intermittently, not by code review — the same "run it for
real, repeatedly" standard the rest of this test suite holds itself to.

Sabotage-verified: temporarily forcing `command_handler.c`'s `default` case to `ACK`
instead of `NACK` makes `command_ack_test` fail as expected — confirms it's actually
exercising the NACK path, not passing vacuously.

All fork real processes and talk over a real Unix socket at `/tmp/comms_i2c.sock` — don't
run them at the same time as each other or as a manually-launched binary using that socket.

## Known gaps

- **`platform/real/drivers/comms_i2c.c` is stale.** It still matches the pre-addressing
  `comms_bus.h` contract (2-argument `send`/`receive`, no `my_address`) and won't compile
  against the current header. Real I2C HAL work is scoped for `docs/roadmap.md` Phase 6;
  updating the stub's signatures to match is a prerequisite that hasn't been done yet.
- **The master's `receive()` reads connections one at a time, blocking per connection.**
  Fine for the current 2-3 node tests; will need `select()`/`poll()`-based multiplexing
  before many subsystems are simultaneously active and one shouldn't be able to stall
  reads from the others.
- **OBC application services** (Telemetry Output, Limit Checker) still don't exist —
  `fdir`'s own `limit_checker` was removed with nothing yet replacing it (blocked on a
  real telemetry pipeline). All 7 OBC processes are real now, see "OBC internal
  architecture" above.
- **A shut-down board has no way back except external intervention.** `fdir` can decide
  to shut a repeatedly-resetting board down, but nothing can un-shut-down it — that
  needs EPS to be able to power-cycle a board, which doesn't exist yet.
- **ADCS's watchdog is POSIX-only.** The independent watchdog thread in
  `apps/adcs/src/manager/fault_manager.c` uses `pthread`, which doesn't exist on bare-metal
  hardware. Real hardware needs an actual watchdog peripheral (e.g. STM32's IWDG) kicked
  directly — unlike `board_reset`/`board_shutdown`, this isn't abstracted yet, since the
  register layout is part-specific rather than ARM-architectural.
- **`CMD_POINT_TO_SUN`, `CMD_SHUTDOWN`, and `CMD_TIME_SYNC` are placeholders on the ADCS
  side.** ADCS ACKs and logs them (`command_handler.c`) but doesn't yet actually point at
  the sun, do anything shutdown-specific beyond calling `board_shutdown()`, or apply the
  synced time anywhere — real behavior for all three needs hardware (an actual sun
  sensor/actuator, an RTC) this session doesn't have yet.
- **`time`'s sync source is the OBC Linux box's own system clock**, not a real RTC/GPS
  reference — fine for proving the sync mechanism works, not for real timekeeping.
  `time_sync.c`'s `known_boards[]` table also includes EPS even though EPS has no
  command handler yet — same intentional "free" scalability as `autonomy.c`'s table;
  the send just goes nowhere until EPS exists.
- **`autonomy.c`'s action table has exactly one entry** (point ADCS to the sun). The
  table-driven shape is built to scale to other subsystems (e.g. telling a thermal board
  to heat the bio-chamber), but nothing else is wired in yet.
- **The real E22 radio driver doesn't live in this repo yet.** `radio.h`/`platform/sim/drivers/radio.c`
  are the shared contract and mock; `platform/real/drivers/radio.c` is being built
  separately and intentionally isn't referenced from `platform/CMakeLists.txt` yet.
- **`IPC_send` itself never retries** (see its own comment: "fail fast, don't retry
  forever") — that's a deliberate transport-level choice, not a bug. Two different callers
  compensate for it where it actually matters: `autonomy_thread` only marks an action
  "done" if the send actually succeeded (so a lost startup race gets retried on the very
  next tick, not a full interval later), and `data`'s `filesystem_stream_file` retries a
  failed chunk send a bounded number of times (a fast reader can outrun `obc_ipc`'s
  connection backlog otherwise — a real bug this caught, see "OBC internal architecture"
  above). Any *new* periodic sender or fast streaming loop should follow one of these
  patterns rather than assuming a single send attempt is enough.
- **`compute`'s codec is a subset of CCSDS 121.0**, not the full standard (no second
  extension option, no reference-sample handling) — the zero-block/Rice/verbatim options
  cover what this project needs. True image-aware compression (CCSDS 122.0, a 2D wavelet
  transform) is a separate, larger piece of work, and doesn't make sense to build against
  today's mock text placeholder photo anyway.
- **`compute`'s single job slot rejects a second concurrent request** (`COMPUTE_STATUS_BUSY`)
  rather than queuing it — right-sized for one photo per balloon flight; a queue/pool is
  straightforward to add later if `compute` ever needs to run more than one job at a time.

## Where to go for more

- `docs/roadmap.md` — full satellite plan, phased task list, architecture decisions and why they were made
- `docs/balloon_launch_plan.md` + `docs/balloon_schedule.md` — November-specific scope, cut list, calendar
- `docs/i2c_sim_transport_plan.md` — the I2C bus design this README summarizes
- `docs/api_contracts.md` — public API for every subsystem, built and planned
- `docs/directory_conventions.md` — where new code goes and why
- `docs/testing.md` — how each test works and how to read its output when it fails


Sincerely, CubeSat