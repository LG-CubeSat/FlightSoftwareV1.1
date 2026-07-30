# System Architecture Overview

## The Three-Layer Design
Our software is organized into three distinct layers to ensure portability and reliability.

### 1. The Mission Logic (Top)
- **Files:** `control_task.c`, `command_handler.c`
- **Role:** The high-level "Brain." It makes decisions based on states and targets.
- **Portability:** This layer contains NO hardware-specific code (no pins, no sockets).

### 2. The Service Layer (Middle)
- **Files:** `ccsds.c`, `crc.c`, `command_queue.c`
- **Role:** The "Language" and "Memory." It provides standardized communication and data safety.

### 3. The Hardware Abstraction Layer / HAL (Bottom)
- **Files:** `v_bus.c`, `spi.c`
- **Role:** The "Body." In simulation, it uses Unix Sockets. On a satellite, it uses SPI/I2C pins.

## Data Flow
1.  **Ingest:** The `SPI Task` (Priority 3) blocks on the `v_bus`, waiting for packets.
2.  **Verify:** It checks the CRC. If valid, it pushes the packet to the `Command Queue`.
3.  **Dispatch:** The `Command Task` (Priority 1) pops the packet, decodes the APID, and updates the system state.
4.  **Control:** The `Control Task` (Priority 2) runs at a deterministic 50Hz, reading the state and executing control math regardless of whether new commands arrived.
