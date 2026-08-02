# Flight Software V1 — Mission: November 1st Balloon Launch

## Mission Overview
This repository contains the flight software stack for the **November 1st Balloon Launch**. The goal is to deploy a high-reliability, hybrid architecture capable of maintaining deterministic control during flight and managing mission-critical autonomy.

## Goal Architecture: The Hybrid CubeSat Stack
We are implementing a professional aerospace architecture modeled after modern CubeSat standards.

### 1. High-Level OBC (On-Board Computer) — Linux
*   **Role:** Mission coordination, telemetry downlink, and complex autonomy.
*   **Environment:** Linux process (POSIX-compliant C/C++).
*   **Key Services:** Command Ingest (CI), Telemetry Output (TO), Time-Tagged Scheduler (TTS), Limit Checker (LC).
*   **Communication:** Uses CSP (CubeSat Space Protocol) over UDP for ground link, SPI (via v-bus) for MCU communication.

### 2. Low-Level MCU (Microcontroller) — FreeRTOS
*   **Role:** Hard real-time deterministic control loops.
*   **Target Hardware:** STM32 (BluePill) for final flight; **POSIX Simulator** for development.
*   **Key Services:** ADCS (50Hz Control), EPS (Power Monitoring), Hardware Watchdog.
*   **Communication:** Uses CSP over SPI (via v-bus) for OBC communication.

### 3. Data Link Layer (CSP — CubeSat Space Protocol)
*   All communication between the Ground, OBC, and MCU uses **CSP (CubeSat Space Protocol)**.
*   **Packet Format:** 4-byte CSP header (priority, dst port, src port, reserved) + payload + CRC-32.
*   **Port-Based Multiplexing:** CSP ports route packets to the correct subsystem (ADCS, EPS, COMMS).
*   **CRC-32:** IEEE 802.3 CRC-32 for data integrity over the SPI bus and UDP link.

## Modes of Operation

### Simulation Mode (SIM)
*   **OBC:** Native Linux process on macOS/Linux development machine.
*   **MCU:** FreeRTOS simulated via `pthreads` on POSIX.
*   **v-bus (SPI):** Unix Domain Socket (`/tmp/v_bus.sock`) simulating SPI master/slave.
*   **Build flag:** `-DHW_MODE=OFF` (default).
*   **Use case:** Development, debugging, CI/CD testing.

### Real Hardware Mode (HW)
*   **OBC:** Linux SBC (e.g., Raspberry Pi) running on flight hardware.
*   **MCU:** STM32 running FreeRTOS bare-metal.
*   **v-bus (SPI):** Real STM32 SPI peripheral, wired to OBC SPI bus.
*   **Build flag:** `-DHW_MODE=ON`.
*   **Use case:** Flight hardware integration, ground testing.

## Directory Structure
*   `/docs` — Architecture and design documents (architecture.md, data_flow.md, api_contracts.md, directory_conventions.md).
*   `/rtos` — FreeRTOS kernel source, configuration, and POSIX/hardware ports.
*   `/platform` — Platform abstraction layer with HW/SIM toggle (`v_bus.h` defines the common API).
*   `/drivers` — Hardware drivers (IMU, magnetometer, radio, sun sensor, STM32 HAL wrappers).
*   `/shared` — Code shared across OBC and MCU (CSP packet format, CRC utilities).
*   `/apps/adcs` — MCU flight code (ADCS task, EPS task, IPC Router, Watchdog).
*   `/apps/obc` — Linux mission coordinator (CI, TO, TTS, LC).
*   `/sys` — System-level services (health monitoring, reset reason tracking).

## Getting Started (Simulator)

### Prerequisites
*   CMake 3.16+
*   C compiler (GCC or Clang)
*   pthread library (standard on Linux/macOS)

### Build and Run
```bash
# Configure (SIM mode by default)
cmake -B build -S .

# Build
cmake --build build

# Run the ADCS simulator
./build/apps/adcs/adcs_sim
```

### Build for Hardware
```bash
cmake -B build -DHW_MODE=ON
cmake --build build
```

## Phase Progress
*   **Phase 0:** Build system, CMake, ADCS app compiling and running.
*   **Phase 1:** Architecture & Design Documents (this repo).
*   **Phase 2+:** OBC services, libcsp integration, RTOS queues, OBC↔MCU communication, system integration.