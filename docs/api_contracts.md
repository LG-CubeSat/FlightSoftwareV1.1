# Flight Software V1 — API Contracts

This document defines the public interface for each subsystem. Every module must adhere to these contracts. New subsystems implement the same interface patterns.

All inter-processor communication uses **CSP (CubeSat Space Protocol)** with CRC-32 integrity checking.

---

## 1. comms_bus (Inter-Processor Bus)

**Location:** `shared/interfaces/comms_bus.h` (contract, medium-agnostic) /
`platform/sim/drivers/comms_i2c.c` (sim) / `platform/real/drivers/comms_i2c.c` (HW)

### Header (`comms_bus.h`)

```c
#ifndef COMMS_BUS_H
#define COMMS_BUS_H

#include <stdint.h>

typedef enum {
    COMMS_BUS_OK = 0,
    COMMS_BUS_ERROR = -1,
    COMMS_BUS_TIMEOUT = -2
} CommsBusStatus_t;

CommsBusStatus_t comms_bus_initialize(int is_master);
int comms_bus_send(const uint8_t *data, uint16_t length);
int comms_bus_receive(uint8_t *buffer, uint16_t max_length);

#endif
```

### Contract

| Function | Direction | Blocking | Description |
|---|---|---|---|
| `comms_bus_initialize(int is_master)` | Setup | Yes | `is_master=1` for OBC (bus master), `is_master=0` for MCU (bus slave). In SIM mode, sets up Unix Domain Socket server/client. In HW mode, the real backend still runs the SPI-era STM32 HAL sequence under the hood (see `platform/real/drivers/comms_i2c.c`) — swapping that for a real I2C HAL sequence is separate, not-yet-done work (`roadmap.md` 1.6-1.8, Phase 6), so don't read the file name as a claim that real I2C is wired up yet. |
| `comms_bus_send(const uint8_t *data, uint16_t length)` | OBC→MCU or MCU→OBC | No | Sends `length` bytes over the bus. Returns number of bytes sent or negative on error. |
| `comms_bus_receive(uint8_t *buffer, uint16_t max_length)` | OBC→MCU or MCU→OBC | Yes | Blocks until data arrives or timeout. Returns bytes received or negative on error. |

### Sim Implementation Details
- Uses Unix Domain Socket (`/tmp/comms_i2c.sock`).
- OBC is the server (binds, listens, accepts).
- MCU is the client (connects).
- Message framing: length-prefix + payload (avoids stream ambiguity).

### HW Implementation Details
- Today: an honest stub (returns `COMMS_BUS_ERROR`) unless `STM32_HAL_AVAILABLE` is defined, in which case it runs SPI HAL calls left over from before the I2C-only bus decision — not real I2C yet.
- Same API as SIM — application code does not change between modes, regardless of which medium is actually wired up underneath.

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
- Produces: Mature commands dispatched to MCU via comms_bus

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
    IPC_ERROR_UNKNOWN_PORT = -3,
    IPC_ERROR_QUEUE_FULL = -4
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
| `ipc_router_initialize` | Initializes comms_bus client, creates all subsystem queues (`adcs_command_queue`, `eps_command_queue`, `comms_command_queue`, `telemetry_queue`), starts the router task. |
| `ipc_router_process_packet` | Parses CSP header, validates CRC-32 via `crc32_validate()`, routes payload to the correct subsystem queue based on destination port. |

### Queue Management Logic

The IPC Router manages four FreeRTOS queues:

```c
QueueHandle_t adcs_command_queue;  // Depth 5, sizeof(csp_packet_t)
QueueHandle_t eps_command_queue;   // Depth 5, sizeof(csp_packet_t)
QueueHandle_t comms_command_queue; // Depth 5, sizeof(csp_packet_t)
QueueHandle_t telemetry_queue;     // Depth 10, sizeof(csp_packet_t)
```

**Queue creation (in `ipc_router_initialize`):**
```c
adcs_command_queue = xQueueCreate(5, sizeof(csp_packet_t));
eps_command_queue  = xQueueCreate(5, sizeof(csp_packet_t));
comms_command_queue = xQueueCreate(5, sizeof(csp_packet_t));
telemetry_queue    = xQueueCreate(10, sizeof(csp_packet_t));
```

**Routing logic (in `ipc_router_process_packet`):**
```c
// 1. Validate CRC
if (!crc32_validate(packet->payload, packet->payload_length)) {
    return IPC_ERROR_CRC;
}

// 2. Look up destination port in routing table
QueueHandle_t *target_queue = routing_lookup(packet->dst_port);
if (target_queue == NULL) {
    return IPC_ERROR_UNKNOWN_PORT;
}

// 3. Send to queue (0 timeout — don't block if queue full)
if (xQueueSend(*target_queue, packet->payload, 0) != pdTRUE) {
    return IPC_ERROR_QUEUE_FULL;
}

return IPC_OK;
```

**comms_bus ISR handler (HW mode) — queue signaling:**
```c
/* Still an SPI peripheral ISR under the hood -- see comms_bus.h's note on
 * platform/real/drivers/comms_i2c.c not being real I2C yet. */
void SPI1_IRQHandler(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (SPI_GetITStatus(SPI1, SPI_IT_RXNE) != RESET) {
        uint8_t byte = SPI_ReceiveData8(SPI1);
        xQueueSendFromISR(comms_bus_rx_queue, &byte, &xHigherPriorityTaskWoken);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

**comms_bus receive (SIM mode) — queue signaling:**
```c
int comms_bus_receive(uint8_t *buffer, uint16_t max_length) {
    // Blocks with timeout, then signals IPC Router task
    ssize_t n = read(bus_fd, buffer, max_length);
    if (n > 0) {
        xQueueNotifyGive(ipc_router_task_handle);
    }
    return (int)n;
}
```

### Routing Table (data-driven)

| Destination Port | Subsystem | Queue Name | Queue Depth |
|---|---|---|---|
| 10 | ADCS | `adcs_command_queue` | 5 |
| 11 | EPS | `eps_command_queue` | 5 |
| 12 | COMMS | `comms_command_queue` | 5 |
| 20 | ADCS Telemetry | `telemetry_queue` | 10 |
| 21 | EPS Telemetry | `telemetry_queue` | 10 |
| 22 | COMMS Telemetry | `telemetry_queue` | 10 |

### Dependencies
- Consumes: Raw bytes from comms_bus, CRC-32 handler
- Produces: Routed payloads to subsystem queues
- Invokes: `crc32_validate()` handler, `xQueueSend()` for queue management

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
| `adcs_task_entry` | FreeRTOS task entry point. Runs at 50Hz. Blocks on `xQueueReceive(adcs_command_queue)` for setpoints from IPC Router. |
| `adcs_apply_setpoint` | Computes PID outputs for reaction wheels to reach the target quaternion. |
| `adcs_get_state` | Returns current attitude quaternion and angular velocity. |

### Queue & Handler Logic

The ADCS task runs a 50Hz control loop:

```c
void adcs_task_entry(void *pvParameters) {
    csp_packet_t cmd;
    adcs_state_t state;

    while (1) {
        // Non-blocking check for command from IPC Router
        if (xQueueReceive(adcs_command_queue, &cmd, pdMS_TO_TICKS(10)) == pdTRUE) {
            // Command received — parse setpoint from CSP payload
            adcs_setpoint_t sp = parse_setpoint(cmd.payload);
            adcs_apply_setpoint(&sp);
        }

        // Read mock IMU data (I2C/SPI driver)
        imu_data_t imu = mock_imu_read();

        // Run PID control loop
        state = adcs_control_loop(imu);

        // Every 200ms, send telemetry
        if (get_tick_count() % 20 == 0) {
            csp_packet_t telem = build_telemetry_packet(state);
            xQueueSend(telemetry_queue, &telem, pdMS_TO_TICKS(10));
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(20)); // 50Hz
    }
}
```

**Key logic:**
- Uses `xQueueReceive` with a 10ms timeout so the task doesn't block forever waiting for a command. It continues its 50Hz control loop regardless.
- Telemetry is sent every 200ms (5Hz telemetry rate) to avoid flooding the comms_bus.
- `vTaskDelayUntil` ensures deterministic 50Hz timing, compensating for execution time.

### Dependencies
- Consumes: Setpoints from `adcs_command_queue`, IMU mock data
- Produces: Telemetry via `telemetry_queue` (picked up by comms_bus ISR handler)
- Invokes: `adcs_apply_setpoint()` PID handler, `crc32_compute()` for outgoing telemetry

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

### Queue & Handler Logic

The EPS task runs a 10Hz control loop and monitors power rails:

```c
void eps_task_entry(void *pvParameters) {
    csp_packet_t cmd;
    eps_state_t state;

    while (1) {
        // Non-blocking check for command from IPC Router
        if (xQueueReceive(eps_command_queue, &cmd, pdMS_TO_TICKS(10)) == pdTRUE) {
            handle_eps_command(cmd.payload);
        }

        // Read mock sensors
        state.vbatt_v = mock_adc_read_battery();
        state.solar_current_a = mock_adc_read_solar();
        state.rail_3v3_v = mock_adc_read_3v3();
        state.rail_5v_v = mock_adc_read_5v();

        // Check for low battery — trigger autonomous safe mode if needed
        if (state.vbatt_v < 6.5f) {
            csp_packet_t alert = build_alert_packet(state);
            xQueueSend(telemetry_queue, &alert, pdMS_TO_TICKS(10));
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(100)); // 10Hz
    }
}
```

**Key logic:**
- Uses `xQueueReceive` with 10ms timeout so the task continues its 10Hz loop even without commands.
- Low battery detection happens inside the EPS task. If `vbatt < 6.5V`, it sends an alert CSP packet to the `telemetry_queue`.
- The OBC Limit Checker picks up this alert and autonomously dispatches `CMD_SAFE_MODE` via the TTS.

### Dependencies
- Consumes: Mock battery/solar data, commands from `eps_command_queue`
- Produces: Housekeeping telemetry via `telemetry_queue` (picked up by comms_bus ISR handler)
- Invokes: `mock_adc_read_*()` handlers, `crc32_compute()` for outgoing telemetry

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

### Handler Logic

The Hardware Watchdog is a FreeRTOS task that monitors OBC health:

```c
void watchdog_task_entry(void *pvParameters) {
    uint32_t last_heartbeat_ms = 0;
    const uint32_t WD_TIMEOUT_MS = 1000;

    while (1) {
        // Block waiting for heartbeat from OBC (via comms_bus)
        uint32_t hb;
        if (xQueueReceive(wd_heartbeat_queue, &hb, pdMS_TO_TICKS(1100)) == pdTRUE) {
            // Heartbeat received within timeout — reset watchdog
            last_heartbeat_ms = get_system_ms();
            pet_hardware_watchdog();  // Kick the hardware WD timer
        } else {
            // Timeout — OBC has stopped sending heartbeats
            wd_status_t status = WD_ERROR_MISSED;

            // Log the event (in real HW, write to non-volatile memory)
            log_reboot_reason("OBC heartbeat timeout");

            // Trigger mock MCU system reboot
            system_reset();
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

**Heartbeat packet format (CSP):**
- Source port: 30 (Ground Link on OBC)
- Destination port: 31 (Watchdog on MCU)
- Payload: 4-byte timestamp (Unix epoch ms)
- CRC-32 over header + payload

**Key logic:**
- The watchdog task runs at 1Hz but uses a 1100ms timeout on `xQueueReceive` to give the OBC a 100ms grace period.
- If the OBC dies (e.g., Linux kernel panic, power glitch), heartbeats stop. The watchdog triggers a reboot after ~1100ms.
- In real hardware, `system_reset()` would write a reboot reason to EEPROM and then assert the NRST pin.
- The OBC sends heartbeats every 500ms (well within the 1000ms timeout).

### Dependencies
- Consumes: Heartbeat packets from OBC via `wd_heartbeat_queue` (populated by IPC Router)
- Produces: System reboot event (mock), reboot reason log
- Invokes: `pet_hardware_watchdog()` handler, `system_reset()` handler