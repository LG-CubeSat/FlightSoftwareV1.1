# Flight Software V1 — Architecture Overview

## System Topology

```
                    ┌─────────────────────────────────────────────────────┐
                    │                  GROUND STATION                       │
                    │  (UDP / TCP — sends CSP packets to OBC)             │
                    └──────────────────────┬──────────────────────────────┘
                                           │  Network (UDP/TCP)
                                           ▼
                    ┌─────────────────────────────────────────────────────┐
                    │         LINUX OBC (On-Board Computer)               │
                    │  Linux process — mission coordination               │
                    │                                                   │
                    │  ┌─────────────────────────────────────────────┐  │
                    │  │         Command Ingest (CI)                  │  │
                    │  │  UDP listener — receives CSP packets        │  │
                    │  │  from ground station                        │  │
                    │  │  Validates CSP CRC, decodes destination      │  │
                    │  └──────────────────┬──────────────────────────┘  │
                    │                     │                               │
                    │  ┌──────────────────▼──────────────────────────┐  │
                    │  │     Time-Tagged Scheduler (TTS)              │  │
                    │  │  Min-Heap sorted by UTC timestamp            │  │
                    │  │  1Hz loop dispatches mature commands         │  │
                    │  └──────────────────┬──────────────────────────┘  │
                    │                     │                               │
                    │  ┌──────────────────▼──────────────────────────┐  │
                    │  │     Limit Checker (LC) / Autonomy            │  │
                    │  │  IF eps_vbatt < 6.5V THEN dispatch SAFE_MODE │  │
                    │  └──────────────────┬──────────────────────────┘  │
                    │                     │                               │
                    │  ┌──────────────────▼──────────────────────────┐  │
                    │  │         v-bus Client (OBC side)              │  │
                    │  │  SPI Master — sends/receives CSP packets     │  │
                    │  │  over simulated SPI (Unix Domain Socket)     │  │
                    │  │  or real SPI (STM32 SPI peripheral)          │  │
                    │  └──────────────────┬──────────────────────────┘  │
                    └─────────────────────┼─────────────────────────────┘
                                          │  SPI Bus (v-bus)
                                          ▼
                    ┌─────────────────────────────────────────────────────┐
                    │       FREERTOS MCU (Microcontroller)                │
                    │  Hard real-time deterministic control               │
                    │                                                   │
                    │  ┌─────────────────────────────────────────────┐  │
                    │  │         v-bus Server (MCU side)              │  │
                    │  │  SPI Slave — receives/sends CSP packets      │  │
                    │  │  over simulated SPI or real STM32 SPI        │  │
                    │  └──────────────────┬──────────────────────────┘  │
                    │                     │                               │
                    │  ┌──────────────────▼──────────────────────────┐  │
                    │  │         IPC Router Task                      │  │
                    │  │  Parses CSP header, validates CRC            │  │
                    │  │  Routes payload to subsystem queue           │  │
                    │  └──────────────────┬──────────────────────────┘  │
                    │                     │                               │
                    │  ┌──────────────────▼──────────────────────────┐  │
                    │  │         ADCS Task (50 Hz)                    │  │
                    │  │  Quaternion setpoints → Reaction Wheel PID   │  │
                    │  └─────────────────────────────────────────────┘  │
                    │                                                     │
                    │  ┌─────────────────────────────────────────────┐  │
                    │  │         EPS Task (10 Hz)                     │  │
                    │  │  Battery voltage, solar array current,       │  │
                    │  │  power rail management                        │  │
                    │  └─────────────────────────────────────────────┘  │
                    │                                                     │
                    │  ┌─────────────────────────────────────────────┐  │
                    │  │     Hardware Watchdog (1000 ms)              │  │
                    │  │  Expects heartbeat from OBC. If missed,      │  │
                    │  │  triggers MCU system reboot                    │  │
                    │  └─────────────────────────────────────────────┘  │
                    └─────────────────────────────────────────────────────┘
```

## Modes of Operation

### Simulation Mode (SIM)
- **OBC:** Native Linux process on macOS/Linux development machine
- **MCU:** FreeRTOS simulated via `pthreads` on POSIX
- **v-bus (SPI):** Unix Domain Socket (`/tmp/v_bus.sock`) simulating SPI master/slave
- **Build flag:** `-DHW_MODE=OFF` (default)
- **Use case:** Development, debugging, CI/CD testing

### Real Hardware Mode (HW)
- **OBC:** Linux SBC (e.g., Raspberry Pi) running on flight hardware
- **MCU:** STM32 (e.g., BluePill) running FreeRTOS bare-metal
- **v-bus (SPI):** Real SPI peripheral on STM32, wired to OBC SPI bus
- **Build flag:** `-DHW_MODE=ON`
- **Use case:** Flight hardware integration, ground testing

## Packet Structure: CSP (CubeSat Space Protocol)

All communication — both uplink (Ground → OBC → MCU) and downlink (MCU → OBC → Ground) — uses **CSP (CubeSat Space Protocol)** as the packet structure.

### Why CSP over CCSDS?
- **CCSDS** defines the wire format (the binary layout on the physical link).
- **CSP** provides the transport layer: CRC validation, port-based multiplexing, routing, and packet framing.
- CSP is the end-to-end packet structure for our system — it wraps application data with CRC and port-based routing, making it the right choice for both the simulated SPI bus and the UDP ground link.
- CSP handles the hard parts (CRC, routing, fragmentation) so the flight software doesn't have to reinvent them.

### CSP Packet Format

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   Priority    |  Destination  |     Source    |    Reserved   |
|   (2 bits)    |   Port (8)    |    Port (8)   |     (2 bits)  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Payload (variable length)                    |
|                         ...                                     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    CRC-32 (4 bytes)                            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

| Field | Bits | Description |
|---|---|---|
| Priority | 2 | QoS priority (0 = highest) |
| Destination Port | 8 | Target port on receiver |
| Source Port | 8 | Origin port on sender |
| Reserved | 2 | Reserved for future use |
| Payload | Variable | Command or telemetry data |
| CRC-32 | 32 | Integrity check over header + payload |

### CSP Port Assignments

| Port | Direction | Subsystem | Description |
|---|---|---|---|
| 0 | Both | Reserved | CSP internal use |
| 10 | OBC → MCU | ADCS Command | Attitude commands from OBC to ADCS |
| 11 | OBC → MCU | EPS Command | Power commands from OBC to EPS |
| 12 | OBC → MCU | COMMS Command | Communication commands |
| 20 | MCU → OBC | ADCS Telemetry | Attitude telemetry from ADCS to OBC |
| 21 | MCU → OBC | EPS Telemetry | Power telemetry from EPS to OBC |
| 22 | MCU → OBC | COMMS Telemetry | Communication telemetry |
| 30 | Both | Ground Link | Ground station uplink/downlink |

## Queue Architecture (MCU Internal IPC)

The MCU uses **FreeRTOS queues** as the primary inter-task communication mechanism. Each subsystem task has a dedicated command queue, and a shared telemetry queue.

```
                    ┌─────────────────────────────────────────────┐
                    │         FreeRTOS MCU Queue Layer             │
                    │                                             │
  IPC Router ──────►│  adcs_command_queue (xQueueCreate)        │
  (routes by        │  eps_command_queue  (xQueueCreate)        │
   dst port)        │  comms_command_queue(xQueueCreate)        │
                    │  telemetry_queue     (xQueueCreate)        │
                    │                                             │
 ADCS Task ◄────────│  xQueueReceive(adcs_command_queue)        │
 (50Hz)             │  xQueueSend(telemetry_queue)              │
                    │                                             │
 EPS Task ◄─────────│  xQueueReceive(eps_command_queue)         │
 (10Hz)             │  xQueueSend(telemetry_queue)              │
                    │                                             │
 COMMS Task ◄───────│  xQueueReceive(comms_command_queue)       │
                    │  xQueueSend(telemetry_queue)              │
                    └─────────────────────────────────────────────┘
```

### Queue Design Logic

1. **One queue per subsystem:** Each subsystem has its own command queue. This prevents one subsystem's flood of commands from starving another's queue.
2. **Shared telemetry queue:** All subsystems send telemetry to a single `telemetry_queue`. The v-bus ISR handler reads from this queue and transmits to the OBC.
3. **Blocking receive with timeout:** Tasks call `xQueueReceive()` with a timeout. If no command arrives within the timeout, the task continues its periodic control loop.
4. **No malloc in ISR context:** The v-bus ISR handler never calls `malloc()`. It only signals the IPC Router task via a FreeRTOS queue notify or semaphore.
5. **Queue depth:** Command queues are depth 5 (enough for burst commands). Telemetry queue is depth 10 (higher throughput).

## Handler Design

The system uses three categories of handlers:

### 1. CRC Validation Handler
- **Location:** Shared between OBC and MCU.
- **Logic:** Every incoming CSP packet passes through `crc32_validate(packet_data, length)`. Returns `true` if CRC-32 matches, `false` otherwise.
- **On failure:** Packet is dropped, error counter increments, Limit Checker is notified if error rate exceeds threshold.
- **On success:** Packet proceeds to routing/processing.

### 2. SPI/v-bus ISR Handler
- **SIM mode:** `v_bus_receive()` blocks with a timeout. When data arrives, it signals the IPC Router task via `xQueueNotifyGive()`.
- **HW mode:** STM32 SPI DMA interrupt triggers ISR handler. ISR reads DMA buffer, validates CRC, and sends payload to the routing queue via `xQueueSendFromISR()`.
- **Key invariant:** ISR never blocks, never calls `malloc()`, never does heavy computation. It only transfers data to a queue or signals a task.

### 3. Hardware Watchdog Handler
- **Location:** MCU only.
- **Logic:** Runs at 1Hz. Expects a heartbeat CSP packet from the OBC every 1000ms.
- **On timeout:** Calls `system_reset()` which triggers a mock MCU reboot.
- **On heartbeat:** Resets the watchdog timer counter.
- **Why it matters:** If the OBC dies (e.g., Linux kernel panic), the MCU doesn't keep running blind — it reboots into a known safe state.

## Key Design Principles

1. **Separation of Concerns:** OBC handles mission-level logic; MCU handles real-time control.
2. **Transport Independence:** The v-bus abstraction allows swapping between Unix Domain Sockets (sim) and real SPI (hardware) without changing application code.
3. **CSP as Primary Protocol:** CSP provides CRC, port multiplexing, and routing for all inter-processor communication.
4. **Static Allocation First:** FreeRTOS tasks use static allocation (`xTaskCreateStatic`) for deterministic memory usage in flight.
5. **Data-Driven Routing:** The IPC Router uses a routing table, not hardcoded if/else chains, so new subsystems can be added without modifying the router.
6. **Queue-Based IPC:** All inter-task communication uses FreeRTOS queues. No global variables, no shared memory without synchronization.
7. **Handler Isolation:** Each handler (CRC, SPI, Watchdog) is a separate module with a single responsibility. Handlers are tested independently.
8. **Scalability:** Each subsystem is self-contained with clear API boundaries. New team members can add a subsystem by implementing the API contract and adding it to the routing table.