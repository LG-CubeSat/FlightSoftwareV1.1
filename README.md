# Flight Software V1: Mission - November 1st Balloon Launch

## Mission Overview
This repository contains the flight software stack for the **November 1st Balloon Launch**. The goal is to deploy a high-reliability, hybrid architecture capable of maintaining deterministic control during flight and managing mission-critical autonomy.

## Goal Architecture: The Hybrid CubeSat Stack
We are implementing a professional aerospace architecture modeled after modern CubeSat standards.

### 1. High-Level OBC (On-Board Computer) - Linux
*   **Role:** Mission coordination, telemetry downlink, and complex autonomy.
*   **Environment:** POSIX-compliant C/C++ (Linux/macOS).
*   **Key Services:** Time-Tagged Scheduler, Limit Checker (Autonomy), and UDP Ground Link.

### 2. Low-Level MCU (Microcontroller) - FreeRTOS
*   **Role:** Hard real-time deterministic control loops.
*   **Target Hardware:** STM32 (BluePill) for final flight; **POSIX Simulator** for development.
*   **Key Services:** ADCS (50Hz Control), EPS (Power Monitoring), and the Hardware Watchdog.

### 3. Data Link Layer (CCSDS Protocol)
*   All communication between the Ground, OBC, and MCU uses the **CCSDS Space Packet Protocol**.
*   **Binary Reliability:** Packed structs, CRC-16 checksums, and APID-based routing.

## Current Status: Software-in-the-Loop (SIL)
To ensure the highest reliability for the November 1st launch, we are using a **Simulator-First** approach.
*   The software currently runs as a native macOS/Linux application.
*   FreeRTOS is simulated using `pthreads` to maintain the 50Hz control heartbeat.
*   This allows us to debug mission logic before committing to hardware.

## Directory Structure
*   `/adcs`: The MCU flight code (Attitude Determination & Control).
*   `/obc`: The Linux-based Mission Coordinator.
*   `/shared`: Common CCSDS protocol definitions and binary serialization logic.
*   `/docs`: System architecture, RTOS simulation guides, and hardware migration paths.

## Getting Started (Simulator)
To run the current ADCS heartbeat simulator:
```bash
cd adcs
mkdir build && cd build
cmake ..
make
./adcs_sim
```
