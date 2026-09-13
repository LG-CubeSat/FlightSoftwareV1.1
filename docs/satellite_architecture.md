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

## 4. Open Items / TBD

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
