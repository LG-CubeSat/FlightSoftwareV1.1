# Flight Software V1 — Testing

## Writing a new test

This project does not require a separate C testing framework. A test is a normal C executable:
it returns `0` when every check passes and a nonzero value when any check fails. CMake builds the
executable, and CTest runs it and interprets that exit status.

The portable tests are useful in both build modes:

| Test | What it exercises | SIM | HW_MODE |
|---|---|---:|---:|
| `frame_codec_test` | I2C-independent wire serialization and malformed-frame rejection | yes | yes |
| `obc_ipc_test` | Unix-socket payloads, source roles, and bounded receive timeout | yes | yes |
| `ssdv_roundtrip_test` | Production JPEG-to-SSDV wrapper plus SSDV decode | yes | yes |
| `compute_async_test` | Data/compute process IPC, busy response, and cancellation | yes | yes |

The remaining tests use the simulated communications bus or launch `adcs_sim`, so they stay
inside `if (NOT HW_MODE)` in `tests/CMakeLists.txt`.

### 1. Write the C test

Create `tests/test_<feature>.c`. Include the public header for the behavior being tested and
call the production API exactly as another component would. Do not include a production `.c`
file directly.

A small test can use this pattern:

```c
#include <stdio.h>
#include "frame.h"

int main(void)
{
    Frame frame = { .dest_addr = 2, .src_addr = 1, .length = 0 };
    uint8_t wire[4 + MAX_FRAME_PAYLOAD];

    if (frame_serialize(&frame, wire, sizeof(wire)) != 4) {
        fprintf(stderr, "frame_test: FAIL\n");
        return 1;
    }

    printf("frame_test: PASS\n");
    return 0;
}
```

Good tests normally cover three things:

1. The ordinary success path.
2. A boundary or malformed input that the API promises to reject.
3. The externally visible result, rather than a private implementation detail.

Always bound a blocking test. Use the API's timeout, a child-process `alarm()`, and/or CTest's
`TIMEOUT` property so one regression cannot hang the entire suite.

### 2. Make production code linkable

CMake links targets, not arbitrary functions. If the code already belongs to a library such as
`frame_codec` or `obc_ipc`, the test can link that library directly. If useful code exists only
inside an application executable, extract it into a small library first.

For example, the SSDV wrapper used to be compiled directly into `obc_compute`. It is now a
reusable target in `apps/obc/compute/CMakeLists.txt`:

```cmake
add_library(obc_ssdv_codec STATIC
    src/ssdv_codec.c
)

target_include_directories(obc_ssdv_codec PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(obc_ssdv_codec PUBLIC ssdv)
```

`PUBLIC` matters here: consumers need both the wrapper's headers and its `ssdv` dependency.
Use `PRIVATE` when only the target itself needs an include path or linked library.

### 3. Register the test in CMake

Add three commands to `tests/CMakeLists.txt`:

```cmake
add_executable(frame_codec_test test_frame_codec.c)
target_link_libraries(frame_codec_test PRIVATE frame_codec)
add_test(NAME frame_codec_test COMMAND frame_codec_test)
set_tests_properties(frame_codec_test PROPERTIES TIMEOUT 5)
```

Each line has one job:

- `add_executable` compiles the test source into a runnable program.
- `target_link_libraries` supplies the production implementation and propagates its public
  include directories. This is why the test can write `#include "frame.h"` without a relative
  path.
- `add_test` registers the executable with CTest. Merely building an executable does not make
  CTest discover it.
- `set_tests_properties(... TIMEOUT 5)` tells CTest to kill it after five seconds.

Keep a portable test outside `if (NOT HW_MODE)`. Put it inside that condition only when it
requires a SIM driver or a SIM application. A future hardware-only test can use
`if (HW_MODE)`, but a physical-device test should normally be labeled separately so an ordinary
Pi build does not try to operate hardware unexpectedly.

### 4. Protect shared test resources

CTest can run tests concurrently with `ctest -j`. Tests using the same fixed socket path must
declare a resource lock:

```cmake
set_tests_properties(obc_ipc_test PROPERTIES
    TIMEOUT 5
    RESOURCE_LOCK obc_ipc_sockets
)
```

Every test with the same lock name runs serially relative to the others holding that lock, while
unrelated tests remain parallel. The suite uses `obc_ipc_sockets` for OBC Unix sockets and
`comms_bus_socket` for `/tmp/comms_i2c.sock`. A test that needs both can specify a semicolon list:

```cmake
RESOURCE_LOCK "comms_bus_socket;obc_ipc_sockets"
```

This is not production locking; it only prevents test processes from stealing each other's
fixed endpoints.

### 5. Configure, build, and run

For the complete simulation suite:

```bash
cmake -S . -B build-sim -DHW_MODE=OFF -DBUILD_TESTS=ON
cmake --build build-sim
ctest --test-dir build-sim --output-on-failure -j4
```

For the portable HW_MODE tests on the Raspberry Pi:

```bash
cmake -S . -B build-hw -DHW_MODE=ON -DBUILD_TESTS=ON
cmake --build build-hw
ctest --test-dir build-hw --output-on-failure -j4
```

On a non-Linux development machine, the complete HW build cannot compile the real Linux I2C
driver. You can still prove that the portable targets are genuinely available in HW_MODE:

```bash
cmake -S . -B build-hw -DHW_MODE=ON -DBUILD_TESTS=ON
cmake --build build-hw --target \
  frame_codec_test obc_ipc_test ssdv_roundtrip_test compute_async_test
ctest --test-dir build-hw --output-on-failure -j4
```

To iterate on one test, build its target and use CTest's regular-expression filter:

```bash
cmake --build build-sim --target frame_codec_test
ctest --test-dir build-sim -R '^frame_codec_test$' --output-on-failure
```

When adding a new test, run it once by itself and then run the whole suite with `-j4`. The
parallel run catches undeclared shared resources and process-cleanup mistakes that a single-test
run cannot reveal.

---

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
