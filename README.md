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
| OBC | No longer a single binary — split into 7 cooperating Linux processes (`supervisor`, `fdir`, `commands`, `compute`, `data`, `mission`, `time`) talking over local IPC, see `apps/obc/roles.md`. `supervisor`, `fdir`, `commands`, and `mission` are all real and working now. `mission` runs a one-shot scripted balloon timeline (ascent → photo → downlink, against mock camera/radio) plus a recurring `autonomy` thread that periodically commands other subsystems (e.g. telling ADCS to point at the sun). `compute`/`data`/`time` are still stubs — Telemetry Output and Time-Tagged Scheduler described in `docs/api_contracts.md` don't exist yet. |
| Comms bus (I2C) | Shared-bus simulation with address-based framing (see below) — multiple nodes on one simulated bus, each filtering to its own traffic. Real I2C HAL backend is still a stub (see Known gaps). |
| EPS / Thermals / Camera / Comms (radio) | Not yet scaffolded. |
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
for the full breakdown of who owns what. The two worth knowing about here:

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

## Repository layout

```
apps/
├── adcs/                  # FreeRTOS MCU app -- reference implementation, now with local fault handling
│   └── src/manager/fault_manager.c   # Watchdog thread, bounds checking, reset/shutdown triggers
└── obc/                   # 7 separate Linux processes, not one binary -- see apps/obc/roles.md
    ├── supervisor/         # Process lifecycle: spawns, reaps, restarts the other 6
    ├── fdir/               # Fault policy: health_monitor tracks board resets, fallback acts on them
    ├── commands/           # Owns the external bus: ingest (inbound routing), relay (outbound)
    ├── compute/ data/ mission/ time/   # Still stubs
    └── ipc/                # Internal IPC shared by all 7

shared/
├── interfaces/
│   ├── comms_bus.h         # The medium-agnostic bus contract
│   ├── board_reset.h       # Board self-reset, sim (re-exec) vs real (Cortex-M AIRCR) impl in platform/
│   ├── board_shutdown.h    # Board halt, sim (exit) vs real (Cortex-M WFI) impl in platform/
│   ├── frame.h / frame.c   # Wire framing (addressing + serialization), shared by every backend
├── csp/                    # CSP-to-transport glue (csp_network.c, csp_if_spi.c, csp_commands.h)

platform/
├── sim/drivers/    # comms_i2c.c, board_reset.c, board_shutdown.c -- what you build against today
└── real/drivers/   # Same three, hardware-backed -- comms_i2c.c is stale, see Known gaps

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
spawns `fdir`, `commands`, and the other roles internally (`commands` is the bus master
and needs to be listening before ADCS connects, though ADCS retries if it isn't yet):

```bash
# terminal 1
./build/bin/obc_supervisor

# terminal 2
./build/apps/adcs/adcs_sim
```

With nothing else driving traffic, this just brings both sides up cleanly and connects
them over the simulated bus. To actually see something happen, send ADCS a command from
`commands`' `relay` (a `relay_request_t` over `IPC_send(ROLE_COMMANDS, ...)`, see
`apps/obc/fdir/src/fallback.c` for a working example building one) — an out-of-range
`CMD_MOVE_TO_POSITION` will walk the whole chain end to end: ADCS detects the fault,
resets itself, notifies the OBC, and after enough repeats `fdir` shuts the board down.

### Build for hardware

```bash
cmake -S . -B build -DHW_MODE=ON
cmake --build build
```

## Testing

```bash
ctest --test-dir build --output-on-failure
```

**Currently broken** — `position_command_test` and `command_ack_test` still depend on an
`obc_sim` build target that no longer exists since the OBC split into 7 processes; CMake
configure for the test suite fails until they're updated, see Known gaps. The other two
still work:

| Test | What it proves |
|---|---|
| `comms_bus_test` | Two nodes (OBC + ADCS), addressed frames flow correctly both directions over the real (non-mocked) transport. |
| `comms_bus_addressing_test` | Three nodes (OBC + two slaves) — a message addressed to one slave is *not* delivered to the other. This is the one that actually exercises the broadcast-and-filter design; two-node tests can't catch a misrouted message since there's nowhere else for it to go. |
| ~~`position_command_test`~~ | ~~Full-stack integration: spawns the real `obc_sim`/`adcs_sim` binaries and verifies a command flows OBC → CSP → bus → ADCS's FreeRTOS tasks → telemetry → back to OBC, repeatedly.~~ Broken, see above. |

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
- **`tests/CMakeLists.txt` still depends on an `obc_sim` target that no longer exists**
  (`position_command_test` and `command_ack_test` both do `add_dependencies(... obc_sim
  adcs_sim)`), left over from before the OBC split into 7 processes. CMake configure for
  the test suite will fail until these are updated to spawn the new per-role binaries
  (`obc_supervisor`, etc.) instead.
- **OBC application services** (Telemetry Output, Time-Tagged Scheduler, Limit Checker)
  still don't exist — `compute`/`data`/`mission`/`time` are stubs, and `fdir`'s own
  `limit_checker` was removed with nothing yet replacing it. `supervisor`, `fdir`, and
  `commands` are real, see "OBC internal architecture" above.
- **A shut-down board has no way back except external intervention.** `fdir` can decide
  to shut a repeatedly-resetting board down, but nothing can un-shut-down it — that
  needs EPS to be able to power-cycle a board, which doesn't exist yet.
- **ADCS's watchdog is POSIX-only.** The independent watchdog thread in
  `apps/adcs/src/manager/fault_manager.c` uses `pthread`, which doesn't exist on bare-metal
  hardware. Real hardware needs an actual watchdog peripheral (e.g. STM32's IWDG) kicked
  directly — unlike `board_reset`/`board_shutdown`, this isn't abstracted yet, since the
  register layout is part-specific rather than ARM-architectural.

## Where to go for more

- `docs/roadmap.md` — full satellite plan, phased task list, architecture decisions and why they were made
- `docs/balloon_launch_plan.md` + `docs/balloon_schedule.md` — November-specific scope, cut list, calendar
- `docs/i2c_sim_transport_plan.md` — the I2C bus design this README summarizes
- `docs/api_contracts.md` — public API for every subsystem, built and planned
- `docs/directory_conventions.md` — where new code goes and why
- `docs/testing.md` — how each test works and how to read its output when it fails


Sincerely, CubeSat