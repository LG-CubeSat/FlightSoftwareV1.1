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
| OBC | No longer a single binary — split into 7 cooperating Linux processes (`supervisor`, `fdir`, `commands`, `compute`, `data`, `mission`, `time`) talking over local IPC, see `apps/obc/roles.md`. `supervisor`, `fdir`, `commands`, `mission`, and `time` are all real and working now. `mission` runs a one-shot scripted balloon timeline (ascent → photo → downlink, against mock camera/radio) plus a recurring `autonomy` thread that periodically commands other subsystems (e.g. telling ADCS to point at the sun). `time` periodically pushes a `CMD_TIME_SYNC` to every known board and can also answer an on-demand sync request. `compute`/`data` are still stubs — Telemetry Output and Time-Tagged Scheduler described in `docs/api_contracts.md` don't exist yet. |
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
    wait for ascent → take photo → downlink → done) — deliberately scripted, no real
    autonomy, since the balloon flight doesn't need or want decision-making.
  - `payload_commander` is what the scheduler (and autonomy) call to actually do
    something: capture a photo (`camera_capture`), downlink a file over the radio
    (`radio_send`), or command another board (e.g. `payload_commander_point_to_sun`
    builds a `command_envelope_t` and hands it to `commands`' relay over
    `obc_relay_protocol.h`).
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

Every long-lived OBC role sends a periodic no-payload heartbeat ping to `supervisor`
(`IPC_send(ROLE_SUPERVISOR, NULL, 0)`, see any role's `heartbeat.c`) — without it,
`supervisor`'s frozen-check can't distinguish "quietly working" from "hung," and will
restart a perfectly healthy process. `fdir`, `commands`, `mission`, and `time` all do this
today; `compute`/`data` will need the same `heartbeat.c` the moment they stop being stubs.

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
    ├── compute/ data/      # Still stubs
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
spawns `fdir`, `commands`, `mission`, `time`, and the other roles internally (`commands`
is the bus master and needs to be listening before ADCS connects, though ADCS retries if
it isn't yet):

```bash
# terminal 1
./build/bin/obc_supervisor

# terminal 2
./build/apps/adcs/adcs_sim
```

With both sides up: `mission` runs its scripted balloon timeline on its own (wait for
"ascent" → mock photo capture → mock radio downlink, logged at each step — no ADCS
involvement); its `autonomy` thread periodically sends ADCS a `CMD_POINT_TO_SUN` through
`commands`' `relay`; and `time` immediately (then periodically) sends ADCS a
`CMD_TIME_SYNC` the same way — watch for `[COMMAND HANDLER] Time sync command received.`
in ADCS's log. To exercise the fault path instead, send ADCS a command built the same way
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

All four tests build and pass. None of them spawn a full OBC process tree (`obc_sim`
doesn't exist anymore, and no single OBC process autonomously polls ADCS the way the old
one-binary OBC did) — instead, `position_command_test` and `command_ack_test` *are* the
OBC's CSP node themselves (the same `csp_network_init(OBC_ADDRESS, 1)` call any real OBC
role makes), talking to a real spawned `adcs_sim`. This tests ADCS's actual command/task
pipeline over the real wire contract without depending on which internal OBC processes
happen to exist:

| Test | What it proves |
|---|---|
| `comms_bus_test` | Two nodes (OBC + ADCS), addressed frames flow correctly both directions over the real (non-mocked) transport. |
| `comms_bus_addressing_test` | Three nodes (OBC + two slaves) — a message addressed to one slave is *not* delivered to the other. This is the one that actually exercises the broadcast-and-filter design; two-node tests can't catch a misrouted message since there's nowhere else for it to go. |
| `position_command_test` | Full-stack happy path: repeated `CMD_MOVE_TO_POSITION` commands get ACKed, activate every task the command should (Control, Estimation, Sensor, Telemetry — not Housekeeping), and get a matching telemetry report back, all repeated across multiple cycles so a "works once then hangs" regression can't slip through. |
| `command_ack_test` | ACK/NACK protocol edge cases `position_command_test` doesn't cover: an unrecognized command_id gets NACKed, an undersized `CMD_MOVE_TO_POSITION` (missing its target) gets NACKed, and a valid command right after both still gets ACKed — proving bad input doesn't wedge the handler. |

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
  `compute`/`data` are stubs, and `fdir`'s own `limit_checker` was removed with nothing
  yet replacing it (blocked on a real telemetry pipeline). `supervisor`, `fdir`,
  `commands`, `mission`, and `time` are real, see "OBC internal architecture" above.
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

## Where to go for more

- `docs/roadmap.md` — full satellite plan, phased task list, architecture decisions and why they were made
- `docs/balloon_launch_plan.md` + `docs/balloon_schedule.md` — November-specific scope, cut list, calendar
- `docs/i2c_sim_transport_plan.md` — the I2C bus design this README summarizes
- `docs/api_contracts.md` — public API for every subsystem, built and planned
- `docs/directory_conventions.md` — where new code goes and why
- `docs/testing.md` — how each test works and how to read its output when it fails


Sincerely, CubeSat