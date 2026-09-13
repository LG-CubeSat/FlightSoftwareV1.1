# Flight Software V1 — Satellite Architecture

**Status:** current as of 2026-09-13. This document replaces the earlier `roadmap.md`,
`balloon_launch_plan.md`, `balloon_schedule.md`, `architecture.md`, `data_flow.md`,
`command_flow.html`, `i2c_sim_transport_plan.md`, and `structure.md` — those described an
earlier, unconfirmed hardware concept (a Zynq/FPGA/neuromorphic-accelerator OBC, a
Zephyr/STM32H743 Comms board, EPS/Camera as their own CSP boards). None of that reflects the
actual hardware. This document describes the **real, confirmed physical architecture** instead.
It is deliberately separate from `README.md` — README covers building/running/testing the
software; this document covers what the satellite physically *is*.

**This is a living document.** More specs (exact part numbers, connector pinouts, battery
protection circuitry, antenna details, etc.) are still coming — every open item is called out
explicitly in "Open Items / TBD" below rather than guessed at.

---

## 1. Physical Hardware Overview

### OBC — Raspberry Pi Zero 2 W

The On-Board Computer is a **Raspberry Pi Zero 2 W**, mounted on a custom PCB alongside the
power circuitry. It runs Linux and hosts the 7-process OBC software stack described in
`README.md`'s "OBC internal architecture" section. It is the CSP bus master and the only node
with a general-purpose OS.

Three things are wired directly to the Pi as local peripherals — **not** as CSP bus nodes:

| Peripheral | Interface | Role |
|---|---|---|
| E22 LoRa module | UART | Ground radio link (uplink/downlink) |
| Arducam camera module | USB-C | Payload photo capture |
| (I2C bus to ADCS + Thermals) | I2C | See §2 below |

### Power — 4S2P 21700 pack + buck converter (no MCU)

Power comes from a **4S2P 21700 Li-ion battery pack** (4 cells in series × 2 in parallel) feeding
a **buck converter** that steps the pack voltage down to a clean, regulated **5V rail** for the
Pi (and, presumably, anything else on the same rail — see Open Items).

This is **not** a smart EPS board. There is no microcontroller, no telemetry, no commandable
power-rail switching — it's passive power-conversion hardware. It has no CSP address and is not
part of the bus software architecture (see §2's note on the reserved EPS address).

### ADCS — STM32 + RTOS board, I2C

A separate physical board carrying an **STM32 microcontroller running an RTOS** (the existing
`apps/adcs` FreeRTOS reference implementation targets this role already). Connects to the OBC
over the shared **I2C** bus as a CSP node.

### Thermals — STM32 + RTOS board, I2C

Same shape as ADCS: a separate **STM32 + RTOS** board on the same I2C bus, its own CSP node.
Software for this board doesn't exist yet (see Open Items / follow-up work).

### Camera — Arducam over USB-C (not a separate board)

The camera is an **Arducam module**, connected to the Pi directly over **USB-C**. It is not a
separate PCB, not a CSP node, and doesn't sit on the I2C bus — it's a USB peripheral of the OBC,
matching the existing `shared/interfaces/camera.h` contract (today mock-only in
`platform/sim/drivers/camera.c`; a real backend needs a USB/V4L2-based driver, not yet written).

### Comms — E22 LoRa module over UART (not a separate board)

The radio is an **E22 LoRa module**, connected to the Pi directly over **UART**. Like the
camera, it is not a separate PCB and not a CSP node — it's a UART peripheral of the OBC,
matching the existing `shared/interfaces/radio.h` contract (today mock-only in
`platform/sim/drivers/radio.c`; a real backend needs a UART driver speaking whatever protocol
the specific E22 variant uses — transparent mode or its command-mode framing, once confirmed).

### Physical topology diagram

```
                    ┌───────────────────────────────────────────┐
                    │   4S2P 21700 battery pack                  │
                    └───────────────────┬───────────────────────┘
                                        │
                                        ▼
                    ┌───────────────────────────────────────────┐
                    │   Buck converter (EPS -- passive, no MCU)  │
                    │   -> clean 5V rail                         │
                    └───────────────────┬───────────────────────┘
                                        │ 5V
                                        ▼
   ┌────────────────┐     UART     ┌─────────────────────────┐     USB-C     ┌───────────────┐
   │  E22 LoRa       │◄───────────►│   OBC -- Raspberry Pi     │◄─────────────►│  Arducam       │
   │  (ground link)  │              │   Zero 2 W (custom PCB)  │                │  (payload cam) │
   └────────────────┘              └────────────┬─────────────┘                └───────────────┘
                                                  │ I2C (CSP bus, OBC = master)
                              ┌───────────────────┴───────────────────┐
                              ▼                                       ▼
                    ┌───────────────────┐                   ┌───────────────────┐
                    │  ADCS board        │                   │  Thermals board    │
                    │  STM32 + RTOS      │                   │  STM32 + RTOS      │
                    └───────────────────┘                   └───────────────────┘
```

---

## 2. CSP Bus / Software Architecture

Only boards with their own MCU and firmware are CSP nodes. That's **OBC, ADCS, and Thermals** —
three nodes on the shared I2C bus, OBC as master. EPS has no MCU (see §1) and is not a node;
Camera and Comms are OBC-local peripherals (USB/UART), not I2C/CSP nodes.

| Node | CSP Addr | Cmd Port | Telem Port | Status |
|---|---|---|---|---|
| OBC | 1 | — | — | done (SIM); real I2C HAL still stale, see README's Known Gaps |
| ADCS | 2 | 10 | 20 | **done** (reference implementation) |
| EPS | 3 | 11 | 21 | **reserved, not a real board** — kept only in case future battery-monitoring hardware is added to that PCB; no software targets this today |
| THERMALS | 4 | 12 | 22 | reserved, not built — same scaffolding pattern as ADCS, not started |

These addresses and ports match what's already defined in `shared/csp/csp_commands.h`
(`OBC_ADDRESS`, `ADCS_ADDRESS`, `EPS_ADDRESS`, `THERMALS`). Port numbers for Thermals (12/20)
follow the same `10 + (address - 2)` / `+10` pattern already used for ADCS/EPS and aren't yet
wired into any code.

**Camera and Comms are not in this table.** `csp_commands.h` still defines unused `CAMERA` (5)
and `COMMS` (6) address constants left over from the earlier, incorrect board-per-peripheral
assumption — see "Known Code/Doc Mismatches" below.

The I2C bus mechanics (shared-bus simulation with address-based framing, real vs. sim driver
split, frame format) are unchanged from what's already documented in `README.md`'s "Architecture
at a glance" section — this document doesn't repeat that.

---

## 3. Known Code/Doc Mismatches (flagged, not yet fixed)

These are places where the current source code still assumes the old, incorrect topology. Not
changed as part of this documentation pass — noted here so they aren't lost:

- **`apps/eps/` scaffolds a CSP-node EPS** (a FreeRTOS app with command/telemetry tasks) that no
  longer matches real hardware — EPS has no MCU at all. Whether to delete, repurpose (e.g. if a
  battery-monitoring board does get added later), or leave as an unused reference is an open
  decision for whenever EPS's actual future is confirmed.
- **`shared/csp/csp_commands.h` still defines `CAMERA` (address 5) and `COMMS` (address 6)** as
  CSP addresses. Neither is a real bus node under the confirmed hardware — both are OBC-local
  peripherals (USB and UART respectively). These constants are currently unused but not removed.

---

## 4. Path to HW_MODE — Bring-Up TODO

**Status:** none of this is done yet. `-DHW_MODE=ON` doesn't even compile today (see A.1).
This section assumes each board's task/control logic (ADCS/Thermals FreeRTOS application code)
is handled separately — everything here is communication, drivers, and build infrastructure:
the plumbing that has to exist before any of that task logic can run on real silicon and talk
to anything else.

The core problem: `HW_MODE=ON` is currently one flag assumed to mean one thing, but it now has
to cover three genuinely different targets — a Linux SBC (OBC) and two bare-metal Cortex-M
boards (ADCS, Thermals) — each needing different real backends and different toolchains.

### A. Blocking fix — do this first, no hardware required

1. **Fix `platform/real/drivers/comms_i2c.c`'s stale signatures.** It still matches the
   pre-addressing `comms_bus.h` contract (`initialize(int)`, `send(data, length)`,
   `receive(buffer, length)` — no address parameters), so it's a straight compile error
   against the current header, not just an unimplemented stub. `-DHW_MODE=ON` fails to build
   at all until this is fixed, independent of everything else below.

### B. Split "HW_MODE" into its real per-target backends (build system)

2. **Split the real I2C backend in two.** One file/target for OBC-as-master (Linux, talks to
   `/dev/i2c-N`) and one for ADCS/Thermals-as-slave (bare-metal STM32 peripheral) — not one
   `platform/real/drivers/comms_i2c.c` trying to be both. Pick the right one per build target
   in CMake.
3. **Wire an ARM cross-compilation toolchain for ADCS/Thermals.** OBC keeps building natively
   (it's Linux ARM — no cross-compiler needed, same as running it on any dev machine). ADCS and
   Thermals need a real `arm-none-eabi-gcc` toolchain + a CMake toolchain file targeting the
   actual STM32 part once it's confirmed (see Open Items). Neither exists in this repo yet.

### C. OBC-side real peripheral drivers (Raspberry Pi)

4. **Real I2C master driver — DONE (OBC side only), not yet verified on real hardware.**
   `platform/real/drivers/comms_i2c.c` now implements `comms_bus_initialize`/`send`/`receive`
   against Linux `i2c-dev` (`/dev/i2c-1`), using per-transaction addressing (`I2C_RDWR` ioctl so
   each message can target a different slave) rather than the fixed-address `I2C_SLAVE` ioctl,
   since OBC talks to two different slaves (ADCS, Thermals) on one bus. The wire frame format
   (`shared/interfaces/frame.c`) didn't change — only the transport underneath.
   Bring-up steps (OS-level, before this code can run for real): `docs/obc_i2c_bringup.md`.

   **The addressing/receive design question below is resolved: option 1.** `receive()`
   round-robin polls each known slave (`known_slaves[]` in the driver, keyed by CSP address from
   `csp_commands.h` and physical I2C address from `platform/real/include/i2c_addresses.h`),
   returning the first one with real data (a `frame.length == 0` reply means "checked in,
   nothing new" and the loop moves on). `comms_bus.h`'s contract and every caller above it are
   unchanged, as intended.

   **Still open:**
   - `platform/real/include/i2c_addresses.h`'s addresses (`0x42`/`0x43`) are placeholders, not
     confirmed against real wiring (ties to D.12 and the Open Items below).
   - Verified so far only by syntax/type-checking against real Linux kernel headers via Docker
     (macOS has no `<linux/i2c-dev.h>` — see `docs/obc_i2c_bringup.md`'s note on this). No
     physical I2C bus has exercised this code yet.
   - **This is currently the only real backend, and `platform/CMakeLists.txt` compiles it for
     every `HW_MODE=ON` target** — item B.2 (splitting OBC-real vs. MCU-real into separate
     files/targets) still hasn't happened, so an ADCS/Thermals HW_MODE build today would
     incorrectly pull in this Linux-only code instead of a real STM32 I2C slave implementation.
5. **Real radio driver (`platform/real/drivers/radio.c` — doesn't exist yet, not even
   referenced in `platform/CMakeLists.txt`'s HW_MODE branch).**
   - Configure the Pi's UART (likely needs `enable_uart=1` and `dtoverlay=disable-bt` in
     `config.txt` on a Pi Zero 2 W, so the E22 gets the full PL011 UART instead of the
     limited mini-UART shared with Bluetooth).
   - Drive the E22's `M0`/`M1` mode-select pins and read its `AUX` status pin over GPIO
     (recommend `libgpiod` over the deprecated sysfs GPIO interface) alongside the UART data
     path.
   - Implement the actual E22 send/receive protocol (transparent vs. configuration mode,
     channel/address setup) — blocked on confirming the exact module variant (Open Items).
6. **Real camera driver (`platform/real/drivers/camera.c` is currently a bare `TODO` stub).**
   - Confirm whether the Arducam enumerates as a standard UVC device (→ straightforward V4L2
     `/dev/videoN` capture) or needs Arducam's own vendor SDK (more work) — blocked on
     confirming the exact model (Open Items).
   - SSDV needs valid JPEG input — if the camera doesn't produce JPEG natively (e.g. MJPEG over
     UVC), a JPEG-encode step (e.g. `libjpeg`) has to happen before handing frames to `compute`.

### D. MCU-side bring-up (ADCS + Thermals boards)

7. **Real I2C slave driver.** The current real backend is vestigial SPI HAL code, not even
   I2C-shaped (it sets fields like `CLKPolarity`/`NSS` that I2C doesn't have) — needs an actual
   STM32 I2C peripheral driven in slave mode, address-matched to that board's assigned I2C
   address (distinct from its CSP address — see D.12).
8. **Startup code, linker script, and vector table for the actual STM32 part.** None of this
   exists in-repo yet. Blocked on the exact part number (Open Items).
9. **FreeRTOS port matching the STM32's Cortex-M core variant.** The repo currently only has a
   working POSIX port (used for SIM); the hardware port needs picking/wiring per the actual
   core (M0/M3/M4/M4F/M7 — depends on the part).
10. **Real watchdog.** `apps/adcs/src/manager/fault_manager.c`'s watchdog thread is
    `pthread`-based today (POSIX-only) — real hardware needs the STM32's IWDG peripheral kicked
    directly. Separate from `board_reset.c`/`board_shutdown.c`, which are already genuinely
    hardware-ready (ARM-architectural registers, not vendor-specific) and just need real-silicon
    testing, not new code.
11. **Flash/debug tooling.** Decide the ST-Link + OpenOCD/STM32CubeProgrammer flow (or
    equivalent) and, ideally, wire it into a CMake/script target rather than a manual process
    everyone re-derives.
12. **Assign real I2C slave addresses.** Pick a 7-bit I2C address per board (a different
    concept from the CSP address already in `csp_commands.h`) and wire it into each board's
    real backend init.

### E. Physical bus / integration (needs answers from Open Items below)

13. Confirm I2C bus speed and pull-up placement/values once wiring is finalized.
14. Confirm what powers ADCS/Thermals — the same 5V rail as the Pi, or their own regulation —
    since it affects buck-converter sizing.

### F. Small code cleanup so the real bus doesn't carry dead traffic

15. `time_sync.c`'s `known_boards[]` and `autonomy.c`'s target table still include EPS —
    harmless today (fire-and-forget sends that just go nowhere), but worth pruning once real
    boards are wired up so nothing periodically addresses a board that will never exist.
16. `csp_commands.h`'s unused `CAMERA`/`COMMS` address constants (see §3) — fine to leave for
    now, but worth removing once you're confident nothing will reference them, so a future
    contributor doesn't mistake them for real bus nodes.

### G. Validation strategy

17. **There is currently zero automated test coverage under `HW_MODE=ON`.** Root
    `CMakeLists.txt` gates `BUILD_TESTS` with `AND NOT HW_MODE`, so none of the existing
    sabotage-verified integration tests build against the real backends at all. Decide a bench
    validation plan (even a manual bring-up checklist) before boards arrive — the SIM test
    suite proves the logic once real drivers exist, but proves nothing about the drivers
    themselves.
18. **Decide how `obc_supervisor` actually starts on the real Pi** (systemd unit vs. manual
    launch) — a small ops task, but worth deciding before "does the ecosystem come up on
    power-on" is a question anyone needs to answer under time pressure.

### Suggested order

A.1 unblocks everything and needs nothing else first. B.2/B.3, C (OBC drivers), and D (MCU
bring-up) can then proceed in parallel by different owners — they don't depend on each other,
only on A.1. E and F can happen any time. G should start as soon as any real driver exists, not
after all of them do.

---

## 5. Open Items / TBD

Flagged explicitly rather than guessed at — more specs are expected:

- Exact STM32 part number(s) for ADCS and Thermals (same part for both, or different?)
- Exact E22 module variant (frequency band/region, transparent vs. command mode) and its UART
  framing/protocol
- Exact Arducam model/interface variant (some Arducam boards are USB-native, others need a
  carrier board — confirm which applies here)
- Battery pack protection: is there a BMS between the 4S2P pack and the buck converter, or is
  protection built into the cells/holder?
- What else (if anything) shares the buck converter's 5V rail besides the Pi — do ADCS/Thermals
  draw their own regulated power independently, or from this same rail?
- I2C bus electrical details: bus speed, pull-up resistor values/location, connector/pinout
  between OBC and each subsystem board
- Physical mounting / PCB stack-up details for the custom OBC board

---

Flight Software V1 — Satellite Architecture. Update this document as confirmed hardware specs
arrive; keep `README.md` (software build/run/status) in sync with any changes here that affect
the CSP node table or peripheral contracts.
