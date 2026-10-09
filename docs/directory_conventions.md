# Flight Software V1 — Directory Structure Conventions

## Top-Level Layout

```
FlightSoftwareV1/
├── CMakeLists.txt              # Root CMake: project definition, subdirectory includes
├── README.md                   # Project overview and getting started guide
├── AGENTS.md                   # Kilo agent instructions (curriculum, architecture)
├── docs/                       # Architecture and design documents
│   ├── satellite_architecture.md # Confirmed physical hardware + CSP node/address table
│   ├── api_contracts.md        # Public API for every subsystem
│   ├── testing.md               # Test harness docs (comms_bus_test, position_command_test)
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
│       └── hw/                 # Hardware BSP
│           ├── adcs/
│           └── thermals/
├── shared/                     # Code shared across OBC and MCU
│   ├── CMakeLists.txt
│   ├── interfaces/             # Every abstract contract, bus + peripheral together
│   │   └── comms_bus.h         # comms_bus API (shared by sim and real; medium-agnostic)
│   └── csp/                    # CSP protocol layer
│       ├── csp_network.c/.h    # CSP bring-up, wraps the comms_bus transport
│       └── csp_if_spi.c/.h     # CSP-to-transport glue (still SPI-named -- see note below)
├── platform/                   # Platform abstraction layer (HW/SIM toggle)
│   ├── CMakeLists.txt
│   ├── sim/                    # Simulation implementations
│   │   └── drivers/
│   │       └── comms_i2c.c     # Unix Domain Socket mock, satisfies comms_bus.h
│   └── real/                   # Hardware implementations
│       └── drivers/
│           └── comms_i2c.c     # Real backend, satisfies comms_bus.h (today: SPI HAL calls, see note below)
├── apps/                       # Application code
│   ├── adcs/                   # MCU flight code (Attitude Determination & Control) -- done, reference implementation
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
│           ├── main.c
│           ├── ci.c            # Command Ingest (to be created)
│           ├── to.c            # Telemetry Output (to be created)
│           ├── tts.c           # Time-Tagged Scheduler (to be created)
│           └── lc.c            # Limit Checker (to be created)
├── sys/                        # System-level services
├── tests/                      # Sanity + integration tests (CTest-registered)
│   ├── test_comms_bus.c        # comms_bus_test -- transport sanity check
│   └── test_position_command.c # position_command_test -- full OBC<->ADCS integration test
└── build/                       # CMake build output (gitignored)
```

### A note on `comms_bus` vs. SPI naming you'll still see in the tree above

`shared/interfaces/comms_bus.h` and both `platform/{real,sim}/drivers/comms_i2c.c`
files were renamed off their old `v_bus`/SPI-flavored names — that part is done. Two
things are **deliberately** still SPI-named, because renaming them would misrepresent
code that hasn't changed yet:

- `shared/csp/csp_if_spi.c/.h` — the CSP-to-transport glue layer. A real `csp_if_i2c.c`
  is planned once the I2C SIM transport work is complete. Until then this file still does.
- `platform/real/drivers/comms_i2c.c`'s HW-mode branch — still literal vendor
  SPI HAL calls (`HAL_SPI_Init`, `SPI1`, `SPI_MODE_MASTER`, ...). I2C doesn't
  even have some of the fields this code sets (`CLKPolarity`, `NSS`), so this
  needs real driver work, not a rename, once real hardware bring-up starts.

## Naming Conventions

### Files
| Pattern | Example | Description |
|---|---|---|
| `*.h` | `comms_bus.h` | Public header (API contract) |
| `*.c` | `comms_i2c.c` | Implementation file |
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
| `communication/` | Bus communication (comms_bus send/receive) |
| `control/` | Control algorithms (PID, quaternion math) |
| `config/` | Configuration files (FreeRTOSConfig.h, etc.) |
| `interfaces/` | Abstract contracts (bus + peripheral), medium-agnostic |
| `sim/` | Simulation implementations |
| `real/` | Hardware implementations |

### Adding a New Subsystem

To add a new subsystem (e.g., `thermals`):

1. Create `apps/thermals/CMakeLists.txt`
2. Create `apps/thermals/include/` for public headers
3. Create `apps/thermals/src/` for implementation
4. Create `apps/thermals/src/tasks/` for the FreeRTOS task
5. Create `apps/thermals/src/communication/` for comms_bus communication
6. Add `add_subdirectory(apps/thermals)` to the top-level `CMakeLists.txt`
7. Implement the API contract defined in `docs/api_contracts.md`
8. Add the subsystem's CSP port to the routing table in `ipc_router.c` (once it exists)

## Adding a New Peripheral Driver

To add a new SPI-attached sensor driver (e.g., `magnetometer`):

1. Create `shared/interfaces/<peripheral>.h` — pure interface, no hardware-specific types
2. Create `platform/sim/drivers/<peripheral>_mock.c` — believable fake values, logs every call
3. Create `platform/real/drivers/<peripheral>_<part>.c` — honest stub until real hardware work lands
4. The interface header must not include hardware-specific types in its public API

## CMake Conventions

- Every directory with source code has a `CMakeLists.txt`
- Library targets use `add_library(<name> STATIC ...)` or `add_library(<name> INTERFACE)`
- Executable targets use `add_executable(<name> ...)`
- Include directories are declared with `target_include_directories`
- Dependencies are declared with `target_link_libraries`
- `HW_MODE` option toggles between simulation and hardware builds
- `target_compile_definitions` sets `HW_MODE` or `SIM_MODE` per target

## CSP Port Assignment Convention

See `satellite_architecture.md`'s CSP node/address table for the authoritative, current
assignment (OBC=1, ADCS=2, EPS=3 [reserved, not a real board], THERMALS=4). Camera and
Comms are OBC-local peripherals (USB/UART), not CSP nodes — see that document. The port
pattern:

- **OBC → MCU commands:** Ports 10–19 (command port = 10 + node's CSP address - 2 for the FreeRTOS subsystems, per the table)
- **MCU → OBC telemetry:** Ports 20–29 (telemetry port = command port + 10)
- **Ground link:** handled directly by the OBC's UART-attached radio, not a CSP port

## Queue & Handler File Placement

### FreeRTOS Queues (MCU)
Queue creation and management lives in the **IPC Router** module (to be
created alongside each subsystem's `command_handler` task):

- **Queue declarations:** `apps/<subsystem>/src/tasks/ipc_router.c` — `QueueHandle_t <subsystem>_command_queue;` etc.
- **Queue creation:** `ipc_router_initialize()` calls `xQueueCreate()` for each queue.
- **Queue access:** Subsystem tasks access queues via their own `.c` files.
- **Queue header:** `apps/<subsystem>/include/tasks/ipc_router.h` exposes the queue handles to other modules.

### Handlers

| Handler | File | Purpose |
|---|---|---|
| CRC Validation | `shared/csp/` (libcsp-provided) | CSP packet integrity checking |
| comms_bus ISR (SIM) | `platform/sim/drivers/comms_i2c.c` | Signal-based wakeup for IPC Router task |
| comms_bus ISR (HW) | `platform/real/drivers/comms_i2c.c` | Peripheral interrupt handler (still SPI DMA today, see note above) |
| Watchdog | `apps/<subsystem>/src/tasks/watchdog.c` | Heartbeat timeout detection and MCU reboot |
| Limit Checker | `apps/obc/src/lc.c` (to be created) | Telemetry threshold evaluation and safe-mode injection |

**Handler design rules:**
1. Handlers must never call `malloc()` or block (for ISR handlers).
2. Handlers return a status code — they don't print directly (use a logging service).
3. Each handler is a separate `.c` file with a matching `.h` header.
4. Handlers are stateless where possible — state is passed in via arguments.
5. All handlers return an enum status (OK, ERROR_*) for consistent error handling.
