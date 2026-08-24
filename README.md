# Flight Software V1

Flight software stack for LG-CubeSat, currently aimed at a **November balloon launch** —
the near-term goal is proving the CSP/I2C bus architecture end-to-end on real hardware
before it carries the full satellite. See `docs/roadmap.md` (full satellite plan) and
`docs/balloon_launch_plan.md` (November-specific scope and cut list) for the complete
picture; this file is the practical "clone it, build it, run it" reference.

## Status snapshot

| Piece | State |
|---|---|
| ADCS | **Done** — reference implementation. FreeRTOS task set, command handling, telemetry, full CSP round-trip with OBC. |
| OBC | CSP hub running (Linux/POSIX sim). Application services (Command Ingest, Telemetry Output, Time-Tagged Scheduler, Limit Checker) **not yet built**. |
| Comms bus (I2C) | **Just landed.** Shared-bus simulation with address-based framing (see below) — multiple nodes on one simulated bus, each filtering to its own traffic. Real I2C HAL backend is still a stub (see Known gaps). |
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

## Repository layout

```
apps/
├── adcs/                  # FreeRTOS MCU app -- done, the reference implementation
└── obc/                   # Linux CSP hub -- application services not yet built

shared/
├── interfaces/
│   ├── comms_bus.h         # The medium-agnostic bus contract
│   ├── frame.h / frame.c   # Wire framing (addressing + serialization), shared by every backend
├── csp/                    # CSP-to-transport glue (csp_network.c, csp_if_spi.c)

platform/
├── sim/drivers/comms_i2c.c    # Unix-socket shared-bus simulation (what you build against today)
└── real/drivers/comms_i2c.c   # Real hardware backend -- currently a stub, see Known gaps

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

Two terminals — OBC first (it's the bus master and needs to be listening before ADCS
connects, though ADCS retries if it isn't yet):

```bash
# terminal 1
./build/apps/obc/obc_sim

# terminal 2
./build/apps/adcs/adcs_sim
```

You'll see OBC issue a position command, ADCS's command handler pick it up and dispatch it
through the control/estimation/sensor/telemetry tasks, and telemetry flow back to OBC —
that's the full CSP round trip over the simulated I2C bus.

### Build for hardware

```bash
cmake -S . -B build -DHW_MODE=ON
cmake --build build
```

## Testing

```bash
ctest --test-dir build --output-on-failure
```

Three tests, each proving something different:

| Test | What it proves |
|---|---|
| `comms_bus_test` | Two nodes (OBC + ADCS), addressed frames flow correctly both directions over the real (non-mocked) transport. |
| `comms_bus_addressing_test` | Three nodes (OBC + two slaves) — a message addressed to one slave is *not* delivered to the other. This is the one that actually exercises the broadcast-and-filter design; two-node tests can't catch a misrouted message since there's nowhere else for it to go. |
| `position_command_test` | Full-stack integration: spawns the real `obc_sim`/`adcs_sim` binaries and verifies a command flows OBC → CSP → bus → ADCS's FreeRTOS tasks → telemetry → back to OBC, repeatedly. |

All three fork real processes and talk over a real Unix socket at `/tmp/comms_i2c.sock` — don't
run them at the same time as each other or as a manually-launched `obc_sim`/`adcs_sim`.

## Known gaps

- **`platform/real/drivers/comms_i2c.c` is stale.** It still matches the pre-addressing
  `comms_bus.h` contract (2-argument `send`/`receive`, no `my_address`) and won't compile
  against the current header. Real I2C HAL work is scoped for `docs/roadmap.md` Phase 6;
  updating the stub's signatures to match is a prerequisite that hasn't been done yet.
- **The master's `receive()` reads connections one at a time, blocking per connection.**
  Fine for the current 2-3 node tests; will need `select()`/`poll()`-based multiplexing
  before many subsystems are simultaneously active and one shouldn't be able to stall
  reads from the others.
- **OBC application services** (Command Ingest, Telemetry Output, Time-Tagged Scheduler,
  Limit Checker) don't exist yet — OBC currently only demonstrates the position-command
  round trip with ADCS, not the ground-facing services described in `docs/api_contracts.md`.

## Where to go for more

- `docs/roadmap.md` — full satellite plan, phased task list, architecture decisions and why they were made
- `docs/balloon_launch_plan.md` + `docs/balloon_schedule.md` — November-specific scope, cut list, calendar
- `docs/i2c_sim_transport_plan.md` — the I2C bus design this README summarizes
- `docs/api_contracts.md` — public API for every subsystem, built and planned
- `docs/directory_conventions.md` — where new code goes and why
- `docs/testing.md` — how each test works and how to read its output when it fails


Sincerely, CubeSat