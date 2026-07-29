# FreeRTOS POSIX Simulator

## Overview
This project utilizes a **Software-in-the-Loop (SIL)** approach by running FreeRTOS on a host machine (macOS/Linux) rather than target hardware during the initial development phases.

## Why use a Simulator?
- **Speed of Development:** No need to flash hardware for every small logic change.
- **Hardware Independence:** Develop flight logic (PID, state machines, command parsing) before the physical board is ready or even chosen.
- **Determinism:** The simulator mimics the RTOS scheduler behavior, allowing us to verify 50Hz control loops and task priorities.
- **Debugging:** Access to standard `printf` and powerful host-side debuggers (LLDB/GDB) without JTAG/SWD complexities.

## How it Works
1. **POSIX Port:** We use a custom `port.c` and `portmacro.h` that use standard POSIX threads (`pthreads`) to simulate the RTOS tick.
2. **Tick Simulation:** The simulator uses `usleep` to approximate the timing defined in `configTICK_RATE_HZ`.
3. **Task Wrapping:** Flight tasks (like `control_task_loop`) are wrapped in standard FreeRTOS `xTaskCreate` calls.

## Current Setup
- **Control Task:** Runs at 50Hz (20ms period) using `vTaskDelayUntil` for deterministic timing.
- **Command Task:** Runs asynchronously, processing incoming serial/SPI packets.
