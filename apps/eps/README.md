# EPS simulator communication guide

EPS collects simulated sensor data, calculates estimates, and reports them to OBC over
CSP. OBC can also request a report and receive ACK/NACK. This guide describes the current
POSIX/FreeRTOS simulator, not a completed hardware EPS or power-switching controller.

## Addresses and command

| Item | Value | Meaning |
|---|---|---|
| OBC address | 1 | Destination for EPS telemetry; accepted command source |
| EPS address | 3 | Destination for EPS commands |
| EPS command port | 11 | Service on EPS that receives commands |
| EPS telemetry port | 21 | Service on OBC that receives EPS reports |
| Request telemetry command | 90 | `EPS_WIRE_COMMAND_REQUEST_TELEMETRY` |

Definitions live in [csp_commands.h](../../shared/csp/csp_commands.h). Ports identify
services on a node; they are not board addresses. Command 90 is the only implemented EPS
action. Reset, shutdown, rail switching, energy-counter reset, and time sync are not supported.

## Files and responsibilities

| File | Responsibility |
|---|---|
| `src/main.c` | Starts the CSP slave, tasks, and command receiver before the scheduler |
| `src/tasks/sensor_task.c` | Publishes readings, sensor validity, timestamp, and sample sequence |
| `src/tasks/estimation_task.c` | Calculates estimates and their validity |
| `src/data/eps_data.c` | Owns shared data and its mutex-protected getters/setters |
| `src/tasks/telemetry_task.c` | Builds reports, owns the atomic request flag, schedules sends |
| `src/communication/command_handler.c` | Receives commands, checks them, dispatches requests, sends ACK/NACK |
| `../../shared/csp/eps_command_client.c` | OBC-side send and acknowledgment validation |
| `../obc/commands/src/main.c` | Routes radio-originated EPS commands through the client |
| `../obc/commands/src/relay.c` | Routes internal OBC EPS requests and returns their result over IPC |
| `../obc/commands/src/ingest.c` | Receives EPS reports and forwards them to Data |
| `../obc/data/src/storage.c` | Receives encoded telemetry records for persistence |
| `../obc/data/src/telemetry_store.c` | Validates and appends records to the binary log |

Sensor, estimation, and telemetry are FreeRTOS tasks. The simulator's CSP router and EPS
command receiver run in POSIX threads. The command receiver does not use `FromISR` APIs
or directly modify the sensor/estimation snapshots.

## Telemetry flow

1. Sensor and estimation tasks publish their own data structures.
2. Telemetry gets private copies through the getters and constructs `eps_telemetry_t`.
3. It opens a connection to OBC address 1, port 21, allocates a CSP packet, copies the
   payload, sets `packet->length`, and calls `csp_send()`.
4. OBC ingest checks the EPS source address and report size. It adds OBC receive metadata
   and forwards an encoded record through IPC to `ROLE_DATA` (role 3).
5. Data validates the record format and appends it to its telemetry log.

The two data getters lock independently. A report is not guaranteed to contain sensor and
estimation snapshots derived from the same sample. Its timestamp and sequence identify the
sensor snapshot only; estimation-source metadata is a future improvement.

### Report fields

| Fields | Meaning and units |
|---|---|
| `sensor_timestamp_ms`, `sensor_sequence` | Sensor time since RTOS startup in milliseconds and sensor publication sequence |
| `valid_sensor_mask`, `valid_estimation_mask` | Bit masks identifying usable fields; bit definitions are in `include/data/eps_data.h` |
| `solar_voltage_v`, `solar_shunt_voltage_v`, `solar_current_a` | Solar voltage/shunt voltage in volts and signed current in amperes |
| `battery_voltage_v`, `battery_shunt_voltage_v`, `battery_current_a` | Battery voltage/shunt voltage in volts and signed current in amperes |
| `battery_temperature_c` | Battery temperature in degrees Celsius |
| `solar_power_w`, `battery_power_w`, `estimated_power_balance_w` | Estimated power values in watts |
| `battery_energy_in_wh`, `battery_energy_out_wh` | Accumulated charging/discharging energy in watt-hours |
| `estimated_state_of_charge_pct` | Estimated remaining energy as a percentage of configured usable capacity |
| `average_solar_power_w`, `average_battery_current_a` | Averages over accepted calculation intervals, in watts/amperes |
| `estimated_time_to_empty_h`, `estimated_time_to_full_h` | Predictions in decimal hours, not split hour/minute/second fields |
| `state_of_flow` | 1 charging, -1 discharging, 0 idle, 2 invalid |

Reports are sent even when measurements or estimates are invalid. Receivers must check the
appropriate mask before using a value; `NAN` alone is not a replacement for validity checks.

### Timing and packet ownership

- The first periodic report is due immediately; later attempts are roughly 1000 RTOS ms apart.
- The task polls the request flag every 20 RTOS ms. This is not a guaranteed response deadline.
- An extra requested report does not restart the periodic timer. A request and a periodic
  deadline can be satisfied by one report.
- Local connection/allocation failures retry at roughly 200 RTOS ms intervals using a newly
  built snapshot. Failed historical reports are not individually queued.
- After `csp_send()`, CSP owns the packet. Do not free or reuse it. This libcsp function
  returns `void`: handing off a packet does not prove remote delivery.
- RTOS time in a desktop simulator need not advance at exactly wall-clock speed.

## Command flow and acknowledgment

A request payload is `command_envelope_t`, containing `command_id` and `seq`. Command
sequences identify requests; they are unrelated to sensor sample sequences. Callers should
choose a new sequence for each new action and reuse it only when retrying that action.

EPS accepts commands only from OBC's CSP address. That source check is not cryptographic
authentication. A truncated envelope is dropped without a reply. A complete envelope with
extra bytes or an unknown ID gets NACK. The OBC client rejects non-envelope sizes locally.

For command 90, the receiver calls `telemetry_request_send()`. That function checks that
the telemetry task was initialized, then sets an atomic Boolean. The task consumes it with
`atomic_exchange(..., false)`. Reading and clearing happen as one operation, avoiding a
lost update between a separate read and clear. Several requests can merge into one pending
send; this is not a queue of one report per command.

EPS returns `command_ack_t` on the same connection: original command ID, original sequence,
and ACK/NACK. OBC keeps that connection open, waits up to 1000 ms for a reply, and checks
its exact size, command ID, sequence, and status. Telemetry uses a separate connection and
may arrive before or after the command acknowledgment.

**ACK means accepted, not transmitted, received, or stored.** NACK means rejected. A timeout
means no valid acknowledgment was received in time; the command may still have executed.
The client does not automatically retry commands.

EPS remembers the last 32 accepted command ID/sequence pairs. Repeated pairs are ACKed
without setting the request flag again. This bounded history is lost on EPS restart; an
older evicted pair is treated as a new request.

### Sending from another OBC role

Use the existing `relay_request_t` from `apps/obc/include/obc_relay_protocol.h`:

```c
command_envelope_t command = {0};
command.command_id = EPS_WIRE_COMMAND_REQUEST_TELEMETRY;
command.seq = next_sequence++; /* Caller-owned sequence for a new request. */

relay_request_t request = {
    .dest_addr = EPS_ADDRESS,
    .dest_port = EPS_CMD_PORT,
    .length = sizeof(command)
};
memcpy(request.payload, &command, sizeof(command));

if (IPC_send(ROLE_COMMANDS, (const uint8_t *)&request, sizeof(request)) < 0) {
    /* The OBC commands process did not accept the IPC transfer. */
}
```

The role must already have initialized IPC. Its existing receive dispatcher should handle
`eps_relay_reply_t` from `ROLE_COMMANDS`, verify `EPS_RELAY_REPLY_MAGIC`, and match the command
ID and sequence. Do not add a competing IPC receiver thread just to wait for this reply.
An IPC transfer succeeding does not itself mean EPS accepted the command.

| Client result | Value | Meaning |
|---|---|---|
| `EPS_COMMAND_ACCEPTED` | 0 | Matching ACK |
| `EPS_COMMAND_REJECTED` | 1 | Matching NACK |
| `EPS_COMMAND_INVALID_REQUEST` | -1 | Incorrect request length or null payload |
| `EPS_COMMAND_CONNECT_FAILED` | -2 | Could not open the local CSP connection |
| `EPS_COMMAND_NO_BUFFER` | -3 | Packet allocation failed |
| `EPS_COMMAND_TIMEOUT` | -4 | No reply within the read timeout |
| `EPS_COMMAND_INVALID_REPLY` | -5 | Wrong reply size, ID, sequence, or status |

Results are defined in `shared/csp/eps_command_client.h`. In an unidentifiable relay request,
the reply command ID/sequence are zero. Radio uplinks use `[destination node][destination port]
[command payload]` inside the existing radio framing. OBC logs the result; acknowledgment
downlink to ground is not implemented.

## Build and run

Run from the repository root. Save files before building; no commit is required.

```sh
cmake -S . -B build-sim -DHW_MODE=OFF -DBUILD_TESTS=ON
cmake --build build-sim --target obc_commands obc_data eps_sim
```

Start one program per terminal, with OBC commands first:

```sh
# Terminal 1: CSP bus master and command/telemetry routing
./build-sim/bin/obc_commands
```

```sh
# Terminal 2: telemetry storage
./build-sim/bin/obc_data
```

```sh
# Terminal 3: EPS node
./build-sim/apps/eps/eps_sim
```

There is no Enter trigger or hardcoded startup command. EPS's listener prints
`[EPS COMMAND] Listening on port 11.` and periodic reports appear in OBC's ingest log.
The OBC supervisor starts its own OBC processes, not EPS; do not run manual copies of
those OBC processes alongside supervisor-managed copies.

Storage defaults to `/tmp/obc_telemetry.bin` in SIM. Set `OBC_TELEMETRY_LOG_PATH` on the
Data process to choose another location. This file is binary and `/tmp` is temporary.
The current native EPS payload is 88 bytes in the tested desktop build, plus an 18-byte
OBC record header, so each EPS record adds 106 bytes there. Other boards also add records.

The large sensor/estimation console dump is off by default. To enable it, reconfigure
with `-DEPS_DEBUG_OUTPUT=ON` and rebuild EPS; use `OFF` to disable it. That setting changes
printing, not whether telemetry is sent.

## Tests and troubleshooting

Stop manual simulations first, then run:

```sh
cmake --build build-sim --target eps_command_client_test eps_communications_test obc_telemetry_test data_telemetry_test
ctest --test-dir build-sim -R '^(eps_command_client_test|eps_communications_test|obc_telemetry_test|data_telemetry_test)$' --output-on-failure
```

- `eps_command_client_test` uses mocked CSP calls to cover ACK/NACK matching, malformed
  replies, invalid request sizes, timeouts, and allocation/connection failures.
- `eps_communications_test` starts real OBC commands, EPS, and Data processes. It checks
  periodic/requested storage, validity/NaN transport, IPC results, simulated radio routing,
  duplicate ACKs, unknown IDs, rejected lengths, sequence wrap, and an unresponsive EPS.
  It keeps logs in a printed `/tmp/eps-communications-XXXXXX` directory and stops its children.
- `obc_telemetry_test` and `data_telemetry_test` cover the OBC record format and persistence.

The EPS integration test returns skip code 77 if another simulation owns its sockets.
CTest resource locks prevent conflicts with other cooperating tests, not manually started
programs. A restricted sandbox may also prevent local socket binds; tests need permission
to use local Unix sockets and loopback UDP.

| Symptom | Check |
|---|---|
| EPS waits at `Slave connecting` | Start OBC commands first; check its bus initialization |
| `forward to role 3 failed` | Start Data and check its IPC initialization; earlier failed forwards are not replayed |
| ACK timeout | Check EPS is running/listening; timeout does not prove the action was never accepted |
| Invalid request for time sync | Expected: EPS does not implement the larger time-sync payload |
| No large EPS console dump | Enable `EPS_DEBUG_OUTPUT`; this does not affect OBC reports |
| Changed code is not running | Save, rebuild the correct target, and restart the correct binary |

## Remaining hardware and reliability work

Native struct copies depend on compiler padding, endianness, and floating-point layout.
Do not hand-pack a command as five bytes just because its two fields total five bytes:
the current receiver expects `sizeof(command_envelope_t)`. Define a versioned serialized
format before communicating across different builds or MCU architectures.

Real sensor drivers, MCU startup/FreeRTOS/CSP integration, hardware power controls, watchdog
and fault recovery, authenticated commands, and durable duplicate history are not supplied
by this simulator. Delivery/storage acknowledgments, retained telemetry replay, exact
sensor-to-estimation sample correlation, and ground acknowledgment downlink also remain
separate work. The source-address check and native struct size check do not establish any
of these properties.
