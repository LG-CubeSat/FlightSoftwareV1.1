# Flight Software V1 — Directory Structure Conventions

## Top-Level Layout

**Location:** `platform/sim/include/v_bus.h` (sim) / `platform/real/spi/ver.h` (HW)

### Header (`v_bus.h`)

```c
#ifndef V_BUS_H
#define V_BUS_H

#include <stdint.h>

typedef enum {
    V_BUS_OK = 0,
    V_BUS_ERROR = -1,
    V_BUS_TIMEOUT = -2
} VBusStatus_t;

VBusStatus_t v_bus_initialize(int is_master);
int v_bus_send(const uint8_t *data, uint16_t length);
int v_bus_receive(uint8_t *buffer, uint16_t max_length);

#endif
```

### Contract

| Function | Direction | Blocking | Description |
|---|---|---|---|
| `v_bus_initialize(int is_master)` | Setup | Yes | `is_master=1` for OBC (SPI master), `is_master=0` for MCU (SPI slave). In HW mode, initializes the STM32 SPI peripheral. In SIM mode, sets up Unix Domain Socket server/client. |
| `v_bus_send(const uint8_t *data, uint16_t length)` | OBC→MCU or MCU→OBC | No | Sends `length` bytes over SPI/v-bus. Returns number of bytes sent or negative on error. |
| `v_bus_receive(uint8_t *buffer, uint16_t max_length)` | OBC→MCU or MCU→OBC | Yes | Blocks until data arrives or timeout. Returns bytes received or negative on error. |

### Sim Implementation Details
- Uses Unix Domain Socket (`/tmp/v_bus.sock`).
- OBC is the server (binds, listens, accepts).
- MCU is the client (connects).
- Message framing: length-prefix + payload (avoids stream ambiguity).

### HW Implementation Details
- Uses STM32 SPI peripheral with DMA for high-throughput, deterministic transfers.
- Same API as SIM — application code does not change between modes.

---

## 2. OBC — Command Ingest (CI)

**Location:** `apps/obc/src/ci.c` (to be created)

### Public API

```c
typedef struct {
    uint32_t src_port;
    uint32_t dst_port;
    uint8_t *payload;
    uint16_t payload_length;
    uint32_t crc32;
} csp_packet_t;

typedef enum {
    CI_OK = 0,
    CI_ERROR_CRC = -1,
    CI_ERROR_PARSE = -2,
    CI_ERROR_UNKNOWN_DST = -3
} ci_status_t;

ci_status_t ci_initialize(udp_socket_t socket);
ci_status_t ci_receive_packet(csp_packet_t *out_packet);
```

### Contract

| Function | Description |
|---|---|
| `ci_initialize` | Creates a UDP socket listener on the configured ground link port. |
| `ci_receive_packet` | Blocks until a CSP packet arrives. Validates CRC-32. Returns parsed CSP header and payload. |

### Dependencies
- Consumes: UDP socket, CSP CRC-32 implementation
- Produces: Parsed `csp_packet_t` for TTS or LC

---

## 3. OBC — Telemetry Output (TO)

**Location:** `apps/obc/src/to.c` (to be created)

### Public API

```c
typedef struct {
    uint32_t src_port;
    uint32_t dst_port;
    uint8_t *payload;
    uint16_t payload_length;
} csp_packet_t;

typedef enum {
    TO_OK = 0,
    TO_ERROR_FORMAT = -1,
    TO_ERROR_SEND = -2
} to_status_t;

to_status_t to_initialize(udp_socket_t socket);
to_status_t to_send_telemetry(uint32_t dst_port, const uint8_t *data, uint16_t length);
```

### Contract

| Function | Description |
|---|---|
| `to_initialize` | Creates a UDP socket for downlink transmission to the ground station. |
| `to_send_telemetry` | Wraps `data` in a CSP packet (header + payload + CRC-32) and sends via UDP. |

### Dependencies
- Consumes: Telemetry data from MCU queues, CSP CRC-32
- Produces: UDP packets to ground station

---

## 4. OBC — Time-Tagged Scheduler (TTS)

**Location:** `apps/obc/src/tts.c` (to be created)

### Public API

```c
typedef struct {
    uint64_t utc_timestamp_ms;
    uint32_t dst_port;          /* MCU destination port */
    uint8_t *payload;
    uint16_t payload_length;
} tts_command_t;

typedef enum {
    TTS_OK = 0,
    TTS_ERROR_FULL = -1,
    TTS_ERROR_INVALID_TIME = -2
} tts_status_t;

tts_status_t tts_initialize(void);
tts_status_t tts_enqueue(const tts_command_t *command);
tts_command_t *tts_pop_mature(uint64_t current_utc_ms);
```

### Contract

| Function | Description |
|---|---|
| `tts_initialize` | Creates the min-heap priority queue. |
| `tts_enqueue` | Inserts a command into the heap sorted by `utc_timestamp_ms`. |
| `tts_pop_mature` | Returns the top command if its timestamp <= `current_utc_ms`, or NULL if no command is mature. |

### Dependencies
- Consumes: Commands from CI, system RTC
- Produces: Mature commands dispatched to MCU via v-bus (SPI)

---

## 5. OBC — Limit Checker (LC)

**Location:** `apps/obc/src/lc.c` (to be created)

### Public API

```c
typedef struct {
    float vbatt_threshold_v;
    float temperature_max_c;
    uint32_t safe_mode_dst_port;
} lc_rule_t;

typedef enum {
    LC_OK = 0,
    LC_ERROR_VIOLATION = -1,
    LC_ERROR_NO_RULE = -2
} lc_status_t;

lc_status_t lc_evaluate(const float vbatt, const float temperature);
```

### Contract

| Function | Description |
|---|---|
| `lc_evaluate` | Checks telemetry against the rule table. Returns `LC_ERROR_VIOLATION` if any threshold is exceeded. |

### Dependencies
- Consumes: Telemetry from MCU queues (EPS housekeeping)
- Produces: Safety commands enqueued into TTS

---

## 6. MCU — IPC Router

**Location:** `apps/adcs/src/ipc_router.c` (to be created)

### Public API

```c
typedef enum {
    IPC_OK = 0,
    IPC_ERROR_PARSE = -1,
    IPC_ERROR_CRC = -2,
    IPC_ERROR_UNKNOWN_PORT = -3
} ipc_status_t;

typedef struct {
    uint32_t src_port;
    uint32_t dst_port;
    uint8_t *payload;
    uint16_t payload_length;
} csp_packet_t;

ipc_status_t ipc_router_initialize(void);
ipc_status_t ipc_router_process_packet(const csp_packet_t *packet);
```

### Contract

| Function | Description |
|---|---|
| `ipc_router_initialize` | Initializes v-bus (SPI) client, creates subsystem queues, starts the router task. |
| `ipc_router_process_packet` | Parses CSP header, validates CRC-32, routes payload to the correct subsystem queue based on destination port. |

### Routing Table (data-driven)

| Destination Port | Subsystem | Queue Name |
|---|---|---|
| 10 | ADCS | `adcs_command_queue` |
| 11 | EPS | `eps_command_queue` |
| 12 | COMMS | `comms_command_queue` |

### Dependencies
- Consumes: Raw bytes from v-bus (SPI)
- Produces: Routed payloads to subsystem queues

---

## 7. MCU — ADCS Task

**Location:** `apps/adcs/src/tasks/adcs_task.c` (to be created)

### Public API

```c
typedef struct {
    float q0, q1, q2, q3;  /* Quaternion (W, X, Y, Z) */
    float wx, wy, wz;        /* Angular velocity (rad/s) */
} adcs_state_t;

typedef struct {
    float q0_setpoint, q1_setpoint, q2_setpoint, q3_setpoint;
} adcs_setpoint_t;

typedef enum {
    ADCS_OK = 0,
    ADCS_ERROR_SETPOINT = -1,
    ADCS_ERROR_WHEEL = -2
} adcs_status_t;

void adcs_task_entry(void *pvParameters);
adcs_status_t adcs_apply_setpoint(const adcs_setpoint_t *setpoint);
adcs_state_t adcs_get_state(void);
```

### Contract

| Function | Description |
|---|---|
| `adcs_task_entry` | FreeRTOS task entry point. Runs at 50Hz. Blocks on `adcs_command_queue` for setpoints from IPC Router. |
| `adcs_apply_setpoint` | Computes PID outputs for reaction wheels to reach the target quaternion. |
| `adcs_get_state` | Returns current attitude quaternion and angular velocity. |

### Dependencies
- Consumes: Setpoints from IPC Router queue, IMU mock data
- Produces: Telemetry (quaternion, wheel speeds) via v-bus (SPI)

---

## 8. MCU — EPS Task

**Location:** `apps/adcs/src/tasks/eps_task.c` (to be created)

### Public API

```c
typedef struct {
    float vbatt_v;
    float solar_current_a;
    float rail_3v3_v;
    float rail_5v_v;
    uint8_t pwr_rail_enabled;
} eps_state_t;

typedef enum {
    EPS_OK = 0,
    EPS_ERROR_LOW_VBAT = -1
} eps_status_t;

void eps_task_entry(void *pvParameters);
eps_state_t eps_get_state(void);
```

### Contract

| Function | Description |
|---|---|
| `eps_task_entry` | FreeRTOS task entry point. Runs at 10Hz. Monitors mock battery and solar arrays. |
| `eps_get_state` | Returns current power system state. |

### Dependencies
- Consumes: Mock battery/solar data, commands from IPC Router queue
- Produces: Housekeeping telemetry via v-bus (SPI)

---

## 9. MCU — Hardware Watchdog

**Location:** `apps/adcs/src/tasks/watchdog.c` (to be created)

### Public API

```c
typedef enum {
    WD_OK = 0,
    WD_ERROR_MISSED = -1,
    WD_ERROR_TRIGGERED = -2
} wd_status_t;

void watchdog_task_entry(void *pvParameters);
```

### Contract

| Function | Description |
|---|---|
| `watchdog_task_entry` | FreeRTOS task entry point. Runs at 1Hz. Expects a heartbeat CSP packet from the OBC every 1000ms. If missed, triggers a mock MCU system reboot. |

### Dependencies
- Consumes: Heartbeat packets from OBC via v-bus (SPI)
- Produces: System reboot event (mock)
```



<tool_call>write
<arg_key>content</arg_key>
<arg_value># Flight Software V1 — Directory Structure Conventions

## Top-Level Layout

```
FlightSoftwareV1/
├── CMakeLists.txt              # Root CMake: project definition, subdirectory includes
├── README.md                   # Project overview and getting started guide
├── AGENTS.md                   # Kilo agent instructions (curriculum, architecture)
├── structure.md                # High-level directory description
├── docs/                       # Architecture and design documents
│   ├── architecture.md         # System topology, modes, CSP protocol
│   ├── data_flow.md            # End-to-end command and telemetry paths
│   ├── api_contracts.md        # Public API for every subsystem
│   └── directory_conventions.md # This file
├── rtos/                       # Real-Time Operating System layer
│   ├── CMakeLists.txt          # Builds FreeRTOS kernel + ports
│   ├── config/                 # FreeRTOS configuration
│   │   └── FreeRTOSConfig.h
│   ├── kernels/                # FreeRTOS kernel source
│   │   ├── include/            # Public kernel headers
│   │   ├── src/                # Kernel implementation
│   │   └── mem/                  # Memory allocation notes
│   └── ports/                  # Hardware/OS abstraction layers
│       ├── posix/              # POSIX simulation port
│       │   ├── port.c
│       │   ├── portmacro.h
│       │   └── utils/
│       └── hw/                 # Hardware BSP (STM32, etc.)
│           ├── adcs/
│           ├── comms/
│           └── thermals/
├── platform/                   # Platform abstraction layer (HW/SIM toggle)
│   ├── CMakeLists.txt
│   ├── sim/                    # Simulation implementations
│   │   ├── include/
│   │   │   └── v_bus.h         # v-bus API (shared by sim and real)
│   │   └── spi/
│   │       └── v_bus.c         # Unix Domain Socket SPI mock
│   └── real/                   # Hardware implementations
│       └── spi/
│           └── spi_driver.h    # Real SPI API (same interface as sim)
├── drivers/                    # Hardware driver layer
│   ├── CMakeLists.txt
│   ├── imu/
│   ├── magnetometer/
│   ├── radio/
│   ├── stm/
│   └── sun_sensor/
├── shared/                     # Code shared across OBC and MCU
│   ├── CMakeLists.txt
│   ├── ipc/                    # Inter-processor communication definitions
│   │   └── csp_packet.h        # CSP packet format (shared)
│   └── protocol/               # CRC and protocol utilities
│       └── crc32.h
├── apps/                       # Application code
│   ├── adcs/                   # MCU flight code (Attitude Determination & Control)
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   ├── communication/
│   │   │   ├── control/
│   │   │   └── tasks/
│   │   └── src/
│   │       ├── main.c
│   │       ├── communication/
│   │       ├── control/
│   │       └── tasks/
│   └── obc/                    # Linux mission coordinator
│       ├── CMakeLists.txt
│       └── src/
│           ├── ci.c            # Command Ingest
│           ├── to.c            # Telemetry Output
│           ├── tts.c           # Time-Tagged Scheduler
│           └── lc.c            # Limit Checker
├── sys/                        # System-level services
│   ├── CMakeLists.txt
│   ├── health/
│   └── platform/
└── build/                      # CMake build output (gitignored)
```

## Naming Conventions

### Files
| Pattern | Example | Description |
|---|---|---|
| `*.h` | `v_bus.h` | Public header (API contract) |
| `*.c` | `v_bus.c` | Implementation file |
| `port.c` | `rtos/ports/posix/port.c` | OS/hardware port layer |
| `portmacro.h` | `rtos/ports/posix/portmacro.h` | Port-specific macros and types |
| `main.c` | `apps/adcs/src/main.c` | Entry point for an executable |
| `CMakeLists.txt` | Every directory | Build configuration |

### Directories
| Directory | Purpose |
|---|---|
| `include/` | Public headers for a module (consumed by other modules) |
| `src/` | Implementation files |
| `tasks/` | FreeRTOS task implementations |
| `communication/` | Bus communication (v-bus send/receive) |
| `control/` | Control algorithms (PID, quaternion math) |
| `config/` | Configuration files (FreeRTOSConfig.h, etc.) |
| `sim/` | Simulation implementations |
| `real/` | Hardware implementations |
| `utils/` | Utility/helper code |
| `ipc/` | Inter-processor communication shared definitions |
| `protocol/` | Protocol utilities (CRC, serialization) |

### Adding a New Subsystem

To add a new subsystem (e.g., `thermals`):

1. Create `apps/thermals/CMakeLists.txt`
2. Create `apps/thermals/include/` for public headers
3. Create `apps/thermals/src/` for implementation
4. Create `apps/thermals/src/tasks/` for the FreeRTOS task
5. Create `apps/thermals/src/communication/` for v-bus communication
6. Add `add_subdirectory(apps/thermals)` to the top-level `CMakeLists.txt`
7. Implement the API contract defined in `docs/api_contracts.md`
8. Add the subsystem's CSP port to the routing table in `ipc_router.c`

## Adding a New Driver

To add a new driver (e.g., `mag_driver`):

1. Create `drivers/magnetometer/mag_driver.h` — pure interface, no hardware-specific types
2. Create `drivers/magnetometer/mag_driver.c` — implementation
3. Create `drivers/magnetometer/mock/` for mock implementation (simulation)
4. Create `drivers/magnetometer/real/` for real hardware implementation
5. The driver header must not include hardware-specific types in its public API

## CMake Conventions

- Every directory with source code has a `CMakeLists.txt`
- Library targets use `add_library(<name> STATIC ...)` or `add_library(<name> INTERFACE)`
- Executable targets use `add_executable(<name> ...)`
- Include directories are declared with `target_include_directories`
- Dependencies are declared with `target_link_libraries`
- `HW_MODE` option toggles between simulation and hardware builds
- `target_compile_definitions` sets `HW_MODE` or `SIM_MODE` per target

## CSP Port Assignment Convention

When adding a new subsystem, assign CSP ports following this pattern:

- **OBC → MCU commands:** Ports 10–19 (increment by 1 per subsystem)
- **MCU → OBC telemetry:** Ports 20–29 (matching command port + 10)
- **Ground link:** Port 30
- **Reserved:** Port 0 (CSP internal)

Example: A new `THERMALS` subsystem would use:
- Command port: 13 (OBC → MCU)
- Telemetry port: 23 (MCU → OBC)

## Queue & Handler File Placement

### FreeRTOS Queues (MCU)
Queue creation and management lives in the **IPC Router** module:

- **Queue declarations:** `apps/adcs/src/tasks/ipc_router.c` — `QueueHandle_t adcs_command_queue;` etc.
- **Queue creation:** `ipc_router_initialize()` calls `xQueueCreate()` for each queue.
- **Queue access:** Subsystem tasks access queues via their own `.c` files (e.g., `adcs_task.c` calls `xQueueReceive(adcs_command_queue, ...)`).
- **Queue header:** `apps/adcs/include/tasks/ipc_router.h` exposes the queue handles to other modules.

### Handlers

| Handler | File | Purpose |
|---|---|---|
| CRC Validation | `shared/protocol/crc32.c` | `crc32_compute()` and `crc32_validate()` — used by OBC and MCU |
| v-bus ISR (SIM) | `platform/sim/spi/v_bus.c` | Signal-based wakeup for IPC Router task |
| v-bus ISR (HW) | `platform/real/spi/spi_isr.c` | STM32 SPI DMA interrupt handler |
| Watchdog | `apps/adcs/src/tasks/watchdog.c` | Heartbeat timeout detection and MCU reboot |
| Limit Checker | `apps/obc/src/lc.c` | Telemetry threshold evaluation and safe-mode injection |

**Handler design rules:**
1. Handlers must never call `malloc()` or block (for ISR handlers).
2. Handlers return a status code — they don't print directly (use a logging service).
3. Each handler is a separate `.c` file with a matching `.h` header.
4. Handlers are stateless where possible — state is passed in via arguments.
5. All handlers return an enum status (OK, ERROR_*) for consistent error handling.