# Flight Software V1 — Data Flow Document

## End-to-End Command Path (Ground → MCU → Ground)

```
GROUND STATION                              LINUX OBC                              FREERTOS MCU
    │                                           │                                        │
    │  1. CSP Packet (UDP/TCP)                 │                                        │
    │──────────────────────────────────────────►│                                        │
    │                                           │                                        │
    │                                           │  2. CI receives CSP packet             │
    │                                           │────────────────────────────────────────►│
    │                                           │  3. Validate CSP CRC-32               │
    │                                           │  4. Decode destination port            │
    │                                           │  5. If MCU destination: enqueue into   │
    │                                           │     TTS priority queue                  │
    │                                           │  6. If OBC-local: process immediately   │
    │                                           │                                        │
    │                                           │  7. TTS 1Hz loop: check if mature      │
    │                                           │◄────────────────────────────────────────│
    │                                           │  8. Pop mature command from heap        │
    │                                           │  9. Build CSP packet for MCU            │
    │                                           │     (src=OBC, dst=MCU port, CRC)       │
    │                                           │ 10. Send CSP packet via SPI (v-bus)     │
    │                                           │────────────────────────────────────────►│
    │                                           │                                        │
    │                                           │                                        │ 11. MCU SPI receives CSP packet
    │                                           │                                        │─────────────────────────────────────►│
    │                                           │                                        │ 12. IPC Router validates CRC-32
    │                                           │                                        │ 13. IPC Router looks up destination
    │                                           │                                        │     port in routing table
    │                                           │                                        │ 14. IPC Router places payload on
    │                                           │                                        │     correct subsystem queue
    │                                           │                                        │ 15. ADCS task unblocks from queue
    │                                           │                                        │ 16. ADCS executes command
    │                                           │                                        │ 17. ADCS produces telemetry
    │                                           │                                        │ 18. ADCS sends telemetry CSP packet
    │                                           │                                        │     back via SPI (v-bus)
    │                                           │                                        │◄─────────────────────────────────────│
    │                                           │ 19. TO reads telemetry from MCU queue   │
    │                                           │ 20. Wrap in CSP packet (src=MCU, dst=OBC)│
    │                                           │ 21. Send CSP packet via UDP to Ground  │
    │◄─────────────────────────────────────────────────────────────────────────────────────│
```

## Step-by-Step Breakdown

### Step 1: Ground Uplink
- Ground station transmits a **CSP packet** via UDP or TCP to the OBC.
- CSP packet format: 4-byte header (priority, dst port, src port, reserved) + payload + CRC-32.
- The ground station sets the destination port to indicate the intended OBC service (e.g., port 30 for ground link).

### Step 2–6: Command Ingest (CI)
- CI thread listens on a UDP socket.
- Upon receiving a CSP packet:
  1. Validate CRC-32 on the CSP packet.
  2. Decode the destination port to determine the target.
  3. If destination port targets the MCU (ports 10–12), enqueue the command into the TTS priority queue.
  4. If destination port targets a local OBC service (e.g., port 30 for ground link, or LC rules), process immediately.
  5. If CRC is invalid, drop the packet and log the error.

### Step 7–8: Time-Tagged Scheduler (TTS)
- TTS runs at 1Hz.
- Maintains a min-heap of time-tagged commands sorted by UTC timestamp.
- Each iteration, compares the top of the heap to the system RTC.
- If the top command's timestamp has matured, pops it from the heap for dispatch.

### Step 9–10: OBC → MCU Transmission (SPI / v-bus)
- OBC wraps the command payload in a CSP packet destined for the MCU.
  - Source port: OBC SPI port
  - Destination port: MCU subsystem port (e.g., 10 for ADCS)
  - CRC-32 computed over header + payload
- Sends the CSP packet through the v-bus (SPI interface).
  - In simulation: writes to Unix Domain Socket (`/tmp/v_bus.sock`).
  - In hardware: writes to STM32 SPI peripheral.

### Step 11–14: MCU IPC Router
- IPC Router task blocks on v-bus receive (SPI read).
- Upon receiving a CSP packet:
  1. Parses the CSP header (src port, dst port, payload length).
  2. Validates CRC-32 over the entire packet.
  3. Looks up the destination port in the routing table.
  4. Sends the payload to the corresponding subsystem queue.
  5. The ADCS task (or EPS task, etc.) unblocks from its queue.

### Step 15–17: MCU Subsystem Execution
- The ADCS task processes the command (e.g., set quaternion setpoint).
- Runs its 50Hz control loop, computing reaction wheel PID outputs.
- Produces housekeeping telemetry (attitude quaternion, wheel speeds, etc.).

### Step 18–21: MCU → OBC → Ground
- ADCS sends telemetry back through v-bus (SPI) to the OBC.
- OBC's TO thread reads telemetry from the MCU queue.
- Wraps it in a CSP packet (src=MCU telemetry port, dst=OBC ground link port).
- Sends it via UDP/TCP to the ground station.

## Telemetry Path (MCU → OBC → Ground)

```
MCU ADCS Task (50Hz) ──► v-bus (SPI) ──► OBC TO Thread ──► UDP ──► Ground Station
     │                      │                │
     │  50Hz loop          │  CSP packet    │  CSP packet
     │  (telemetry)        │  with CRC-32   │  formation
     ▼                      ▼                ▼
 Housekeeping         SPI transfer      CSP downlink
 data struct          (Unix socket       frame with CRC-32
                       or real SPI)
```

## Anomaly Path: Limit Checker → Safe Mode

```
EPS Task (10Hz) ──► v-bus (SPI) ──► OBC TO ──► LC evaluates ──► TTS enqueues CMD_SAFE_MODE
     │                    │                │                      │
     │  Reads mock        │  CSP packet    │  IF vbatt < 6.5V  │
     │  battery voltage   │  with CRC-32   │  → inject command  │
     ▼                    ▼                ▼                      ▼
 Housekeeping      SPI transfer     Telemetry read     TTS dispatches at
 data              to OBC           from MCU queue     maturity → MCU safe mode
```

## Queue & Handler Logic

### FreeRTOS Queue Architecture (MCU)

The MCU uses four FreeRTOS queues for internal IPC:

| Queue | Type | Depth | Blocking Behavior |
|---|---|---|---|
| `adcs_command_queue` | xQueueCreate | 5 | ADCS task blocks on `xQueueReceive()` with 100ms timeout |
| `eps_command_queue` | xQueueCreate | 5 | EPS task blocks on `xQueueReceive()` with 100ms timeout |
| `comms_command_queue` | xQueueCreate | 5 | COMMS task blocks on `xQueueReceive()` with 100ms timeout |
| `telemetry_queue` | xQueueCreate | 10 | v-bus ISR handler reads with `xQueueReceiveFromISR()` |

**Key design decisions:**
- **Separate command queues:** Prevents one subsystem's burst traffic from starving another. If ADCS floods the bus, EPS still gets its 10Hz commands.
- **Shared telemetry queue:** Downstream (OBC) is the only consumer of telemetry, so a single queue works. If multiple consumers are needed later, use an `xQueueSet` instead.
- **Blocking with timeout:** Tasks use `xQueueReceive(queue, item, pdMS_TO_TICKS(100))`. If no command arrives within 100ms, the task continues its periodic control loop. This prevents a stuck queue from deadlocking the entire task.

### IPC Router Logic

The IPC Router is a FreeRTOS task that runs at the highest priority (ensuring packets are processed immediately). Its logic:

```
1. Block on v_bus_receive() (with 50ms timeout)
2. If data received:
   a. Parse CSP header (4 bytes)
   b. Validate CRC-32 over entire packet
   c. Look up destination port in routing table
   d. xQueueSend(payload, destination_queue, 0)
   e. If queue full: increment overflow counter, send NACK to OBC
3. If timeout: continue loop (watchdog pet)
4. If CRC invalid: drop packet, log error, notify LC
```

**Routing table is data-driven:**
```c
typedef struct {
    uint32_t dst_port;
    QueueHandle_t *queue;
    const char *subsystem_name;
} ipc_route_t;

static const ipc_route_t routing_table[] = {
    { 10, &adcs_command_queue, "ADCS" },
    { 11, &eps_command_queue,  "EPS"  },
    { 12, &comms_command_queue,"COMMS"},
    { 0,  NULL,                 NULL   }  /* sentinel */
};
```

To add a new subsystem, append one line to this table — no other code changes needed.

### OBC Queue Logic

The OBC uses three queues:

| Queue | Type | Purpose |
|---|---|---|
| `tts_heap` | Min-heap (custom) | Stores time-tagged commands sorted by UTC timestamp |
| `tts_dispatch_queue` | xQueueCreate (10) | Mature commands ready for v-bus transmission |
| `mc_rx_queue` | xQueueCreate (20) | Received telemetry from MCU, waiting for TO thread |

**TTS logic:**
```
1Hz loop:
  top = heap_peek(tts_heap)
  if top != NULL && top.utc_timestamp_ms <= get_rtc_ms():
      cmd = heap_pop(tts_heap)
      xQueueSend(tts_dispatch_queue, cmd, 0)
```

**CI logic:**
```
CI thread (blocking):
  packet = recvfrom(udp_socket)
  if crc32_validate(packet):
      if is_mcu_destination(packet.dst_port):
          heap_push(tts_heap, packet)
      else:
          process_locally(packet)
  else:
      crc_error_counter++
      if crc_error_counter > threshold:
          lc_notify_crc_storm()
```

### Handler Logic

#### CRC Validation Handler
```c
bool crc32_validate(const uint8_t *data, size_t length, uint32_t expected_crc) {
    uint32_t computed = crc32_compute(data, length);
    return (computed == expected_crc);
}
```
- Called by: OBC CI, MCU IPC Router, Ground Station (on downlink).
- On failure: Drop packet, increment error counter, optionally trigger LC rule.

#### SPI/v-bus ISR Handler (HW mode)
```c
void SPI1_IRQHandler(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (SPI_GetITStatus(SPI1, SPI_IT_RXNE) != RESET) {
        uint8_t byte = SPI_ReceiveData8(SPI1);
        xQueueSendFromISR(v_bus_rx_queue, &byte, &xHigherPriorityTaskWoken);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```
- Called by: STM32 SPI interrupt (HW mode) or signal handler (SIM mode).
- Never calls `malloc()`, never blocks. Only pushes bytes to a queue and yields.

#### Hardware Watchdog Handler
```c
void watchdog_task_entry(void *pvParameters) {
    uint32_t last_heartbeat = 0;
    const uint32_t timeout_ms = 1000;

    while (1) {
        if (xQueueReceive(wd_heartbeat_queue, &last_heartbeat, pdMS_TO_TICKS(1100)) == pdTRUE) {
            // Heartbeat received within timeout
            pet_watchdog();
        } else {
            // Timeout — OBC is dead
            system_reset();  // Mock MCU reboot
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```
- The OBC sends a heartbeat CSP packet every 500ms (well within the 1000ms timeout).
- If the OBC dies, the watchdog triggers a reboot after ~1100ms.
- The heartbeat is a simple CSP packet with src=OBC, dst=MCU, port=31 (watchdog).

## CRC Integrity

Every CSP packet carries a CRC-32 checksum covering the entire packet (header + payload). This ensures data integrity over the noisy SPI bus (simulated or real).

- **CRC-32 algorithm:** Standard IEEE 802.3 CRC-32 (polynomial 0xEDB88320).
- **Validation points:**
  1. OBC CI validates CRC on incoming ground uplink packets.
  2. MCU IPC Router validates CRC on incoming SPI packets.
  3. OBC TO does not re-validate CRC on outgoing downlink (it generated the packet).
  4. Ground station validates CRC on received downlink packets.
- **Error handling:** If CRC fails at any point, the packet is dropped and an error is logged. If the error rate exceeds a threshold (e.g., >10% CRC failures in 1 minute), the Limit Checker autonomously dispatches a safe-mode command.