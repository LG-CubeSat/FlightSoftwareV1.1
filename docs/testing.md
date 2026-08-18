# Flight Software V1 — Testing

## comms_bus_test — OBC ↔ ADCS Communication Sanity Check

### What it is

`tests/test_comms_bus.c` is a small, self-contained check that the comms bus
transport (`platform/sim/drivers/comms_i2c.c`) actually works — the same code
`obc_sim` and `adcs_sim` use to talk to each other over the simulated bus (a
Unix domain socket at `/tmp/comms_i2c.sock`).

It does **not** mock anything. It forks two processes in-test — one calling
`comms_bus_initialize(1)` (OBC/master role) and one calling
`comms_bus_initialize(0)` (ADCS/slave role) — and drives the real
`send()`/`receive()` functions between them, exactly like the two real
binaries do. If the master/slave handshake, the socket, or the framing
breaks, this test breaks with it.

```
        fork()                          fork()
          │                                │
          ▼                                ▼
   ┌─────────────┐   "PING from OBC"  ┌─────────────┐
   │  run_slave  │◄───────────────────│  run_master │
   │  (ADCS)     │                    │  (OBC)      │
   │             │───────────────────►│             │
   └─────────────┘  "PONG from ADCS"  └─────────────┘
          │                                │
          └────────────► both must PASS ◄──┘
```

Each side has a 5-second watchdog (`alarm()`), so a broken bus fails loudly
within 5 seconds instead of hanging forever.

### Why it exists

On 2026-08-04 the bus silently broke on macOS: a socket type
(`SOCK_SEQPACKET`) that isn't supported there caused `comms_bus_initialize()`
to fail immediately, but nothing checked the return value — so both `obc_sim`
and `adcs_sim` kept running with a dead bus and no error message. `adcs_sim`
looked fine (its Control loop doesn't use the bus), `obc_sim` looked "frozen."
This test exists so that specific failure mode — and anything like it — shows
up as an immediate, obvious `FAIL` instead of hours of silent debugging.

---

## How to run it

**One-time setup** (if you haven't configured a build yet):

```bash
cmake -S . -B build -DHW_MODE=OFF
```

**Build and run** (do this any time you want to check the bus):

```bash
cmake --build build --target comms_bus_test && ./build/tests/comms_bus_test
```

That's it — no need to launch `obc_sim`/`adcs_sim` in separate terminals.
If you've already built the whole project (`cmake --build build`), the test
binary is already built, and you can just run:

```bash
./build/tests/comms_bus_test
```

### Alternative: via CTest

The test is also registered with CTest, if you prefer that entry point (e.g.
scripting it into CI later):

```bash
ctest --test-dir build -R comms_bus_test --output-on-failure
```

---

## Reading the output

### Pass

```
[COMMS BUS] Slave connecting..
[COMMS BUS] Master waiting for Slave...
[COMMS BUS] Slave connected to Master.
[COMMS BUS] Master accepted connection from Slave.
[ADCS] PASS: received 13 bytes, sent matching 14-byte reply
[OBC]  PASS: sent 13 bytes, received matching 14-byte reply
comms_bus_test: PASS
```

Exit code `0`. Both directions of the bus (OBC→ADCS and ADCS→OBC) delivered
the exact bytes that were sent. The transport is healthy.

### Fail

```
[ADCS] FAIL: comms_bus_initialize failed
[OBC] FAIL: comms_bus_initialize failed
comms_bus_test: FAIL (slave_ok=0 master_ok=0)
```

Exit code `1`. The message tells you which side failed and at which step:

| Message | Meaning |
|---|---|
| `comms_bus_initialize failed` | `socket()`/`bind()`/`listen()`/`accept()`/`connect()` failed — check the `[COMMS BUS] ... failed: <reason>` line printed just above it for the actual `errno` string. |
| `send() did not return the expected length` | The write didn't transmit the full message — check for a connection drop. |
| `did not receive the expected message/reply` | Bytes arrived but didn't match what was sent — a framing or corruption bug. |
| Test hangs ~5s then fails | One side is stuck (e.g. blocked in `accept()`/`connect()`/`read()` and never unblocks) — the `alarm()` watchdog kills it and the run is reported as failed rather than hanging forever. |

If it fails, first make sure no leftover `obc_sim`/`adcs_sim` process is
holding `/tmp/comms_i2c.sock` (see below), then re-run.

---

## Things to know

- **Don't run it alongside the real binaries.** It uses the exact same
  hardcoded socket path (`/tmp/comms_i2c.sock`) as `obc_sim`/`adcs_sim`.
  Running it at the same time as either of those will cause spurious
  failures on both sides. If you get a confusing failure, check for
  stragglers:
  ```bash
  pkill -f build/apps/obc/obc_sim
  pkill -f build/apps/adcs/adcs_sim
  rm -f /tmp/comms_i2c.sock
  ```
- **SIM-only.** The test is only built when `HW_MODE=OFF` (the default) —
  there's no real hardware to test against in `HW_MODE=ON` builds yet.
  Controlled by `option(BUILD_TESTS ...)` in the top-level `CMakeLists.txt`,
  default `ON`.
- **Isolated on purpose.** It lives entirely in `tests/`, only links against
  the `platform` library, and doesn't touch FreeRTOS, CSP, or either app's
  `main.c`. Nothing else in the build depends on it — it's safe to ignore,
  extend, or delete without affecting `obc_sim`/`adcs_sim`/`adcs_rtos`.
- **When to run it:** any time you touch `platform/sim/drivers/comms_i2c.c`,
  or whenever `obc_sim`/`adcs_sim` seem to not be talking to each other and
  you want to know in 5 seconds whether the bus itself is the problem before
  digging into FreeRTOS task logic.
