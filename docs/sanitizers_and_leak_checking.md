# Flight Software V1 — Sanitizers & Leak Checking

**Status:** everything in this document was verified by hand on this project's own build during
development (dates/output below are from that session) — not copied from generic sanitizer
documentation. Where something is Linux-specific and wasn't actually run on Linux, that's called
out explicitly rather than assumed.

## What this is for

`obc_sim`/`adcs_sim` and the test suite are host-native SIM builds — the one place this project
can run under compiler instrumentation that catches entire classes of bugs before they reach real
hardware:

- **AddressSanitizer (ASan)** — buffer overflows, use-after-free, double-free.
- **UndefinedBehaviorSanitizer (UBSan)** — signed integer overflow, misaligned access, and other
  undefined behavior the C standard allows a compiler to miscompile silently.
- **Leak checking** (LeakSanitizer on Linux, `leaks` on macOS) — allocations that are never freed.
  This is not academic for this project: `balloon_launch_plan.md`'s Phase B6.1 calls for a
  multi-hour unattended soak test before flight, and a slow leak invisible in a 13-second test run
  is exactly the kind of bug that only surfaces hours into a real flight.

None of this applies to `HW_MODE=ON` builds — it's host-compiler instrumentation, it doesn't
cross-compile to the STM32 target, and wouldn't fit in flash if it did.

## Two switches, kept deliberately separate

```
-DENABLE_SANITIZERS=ON     # ASan + UBSan
-DENABLE_LEAK_CHECK=ON     # LeakSanitizer (Linux) / `leaks` (macOS)
```

They are **not** one switch, because on macOS they conflict at a mechanical level (see the macOS
section) — trying to set both together on Apple hosts is a hard `FATAL_ERROR` at configure time,
not a silent downgrade. On Linux they can be combined freely.

```bash
# memory/UB bugs
cmake -S . -B build-asan -DHW_MODE=OFF -DENABLE_SANITIZERS=ON
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure

# leak checking
cmake -S . -B build-leakcheck -DHW_MODE=OFF -DENABLE_LEAK_CHECK=ON
cmake --build build-leakcheck
ctest --test-dir build-leakcheck --output-on-failure
```

Keep these as separate build directories from your normal `build/` — they're slower and noisier,
meant to be run deliberately (before finishing a feature, after touching `memcpy`/buffer code),
not on every edit-compile cycle.

---

## Linux

**Not independently verified on this machine** (this was all developed on macOS) — the behavior
below follows from how ASan/LSan are documented to work on Linux, and from reading this project's
own CMake logic, but hasn't been run on a real Linux box or CI. Verify before fully trusting it.

### What works

- **`ENABLE_SANITIZERS`** — full ASan + UBSan, no known project-specific caveats. This project's
  FreeRTOS POSIX port (`rtos/ports/posix/port.c`) creates each task as a real `pthread_create()`'d
  OS thread, not a `ucontext`/`makecontext`-swapped fiber — the kind of RTOS simulator design that
  actively confuses ASan's stack tracking. This one doesn't need any fiber-registration
  workarounds to get accurate results.
- **`ENABLE_LEAK_CHECK`** — LeakSanitizer, compiled directly into the binary (`-fsanitize=leak`,
  combined with `address,undefined` if `ENABLE_SANITIZERS` is also on). `tests/CMakeLists.txt`
  additionally forces `ASAN_OPTIONS=detect_leaks=1` on each test's environment rather than relying
  on the local default. Because this is linked-in instrumentation — not a separate external
  process attaching to inspect memory, unlike macOS's `leaks` — it has none of the fork-related
  problems described below. All 4 registered tests should get real leak coverage on Linux,
  including `position_command_test`/`command_ack_test`'s forked-and-exec'd `obc_sim`/`adcs_sim`
  children, since `ASAN_OPTIONS` is an ordinary environment variable inherited across `fork()` +
  `exec()`.
- Note: ASan's leak detection defaults to *on* on Linux even without `ENABLE_LEAK_CHECK` — so
  `ENABLE_SANITIZERS` alone likely already catches leaks there. `ENABLE_LEAK_CHECK` is what this
  project uses to make that explicit (`ASAN_OPTIONS=detect_leaks=1`) rather than depend on
  whatever the local toolchain's default happens to be.

### Limitations

None found specific to this codebase. General caveats apply: the build is slower, and because
sanitizer flags are added globally (`add_compile_options` in the top-level `CMakeLists.txt`),
they also instrument vendored code (libcsp, FreeRTOS kernel, libyaml/libzmq if statically linked)
— noise from third-party code is possible, though none was seen during development.

---

## macOS

Everything below was verified by hand (Apple clang 21, arm64-darwin, macOS 26.5.2) during this
project's development.

### What works

- **`ENABLE_SANITIZERS`** — ASan + UBSan compile and run cleanly, same as Linux, no caveats found.
- **`ENABLE_LEAK_CHECK`**:
  - `sim_leak_check_macos` (a CTest entry, macOS-only) leak-checks `obc_sim`/`adcs_sim` **directly
    and live** — it launches both, lets them exchange real command/ACK/telemetry traffic for a few
    seconds, then attaches `leaks <pid>` to each while they're still running. Verified against
    both a deliberately-leaked `malloc()` (correctly caught, full stack trace) and this project's
    actual binaries (correctly reports 0 leaks, consistent with the buffer-leak bugs already fixed
    earlier in development).
  - `tests/leak_check_macos.sh` wraps a single binary with `leaks --atExit` for manual use. Safe
    **only** for a program that does not itself call `fork()` — see the limitations below for why.

### Limitations (all verified by hand)

1. **LeakSanitizer itself does not exist on macOS.** `clang -fsanitize=leak` fails outright:
   `unsupported option '-fsanitize=leak' for target 'arm64-apple-darwin25.5.0'`. `leaks` is a
   completely different mechanism — an external process that attaches to and inspects another
   process's live heap — not compiler instrumentation.
2. **MemorySanitizer does not exist on macOS at all**, for any use case. Same
   `unsupported option '-fsanitize=memory'` error. There is no substitute wired up in this
   project — catching genuinely uninitialized-memory reads here would require running the same
   `ENABLE_SANITIZERS` build on Linux (or CI). The closest native fallback is compile-time only:
   `-Wall -Wextra -Wuninitialized`, or Clang's static analyzer (`scan-build`).
3. **ASan and `leaks` cannot be used together, ever, on the same binary.** ASan replaces the
   memory allocator with its own instrumented version; `leaks` cannot introspect it:
   `target process is using Address Sanitizer which doesn't work with memory analysis tools`,
   `[fatal] unable to inspect heap ranges of target process`. This is why `ENABLE_SANITIZERS` and
   `ENABLE_LEAK_CHECK` are mutually exclusive on macOS at configure time (see top-level
   `CMakeLists.txt`) — there is no flag combination that gets both at once here.
4. **`leaks` needs `MallocStackLogging=1` set in the *target* process's environment, or it fails
   silently.** Without it, `leaks` reports `0 leaks for 0 total leaked bytes` even when a real,
   deliberate leak is present — a false negative, not an error message. Both
   `leak_check_macos.sh` and `sim_leak_check_macos.sh` set this correctly already; if you ever run
   `leaks` by hand outside these scripts, remember to set it yourself.
5. **`leaks --atExit` cannot safely wrap anything that calls `fork()`.** Attaching as the target's
   direct parent (via macOS's task-port debugging mechanism) corrupts the target's own
   `fork()`/`WEXITSTATUS()`-based exit-status reporting for its children — this is not a theory,
   it was reproduced directly against this project's own `comms_bus_test`: running it plainly, or
   with just `MallocStackLogging=1` set, passes; running the identical binary under
   `leaks --atExit` makes it report an internal `FAIL` that isn't real. Compounding this,
   `leaks --atExit`'s own exit code reflects only *its* leak result, not the wrapped binary's real
   exit code — so blindly wrapping a forking test would have silently reported that corrupted
   `FAIL` as a passing CTest run. **This is why `comms_bus_test`, `comms_bus_addressing_test`,
   `position_command_test`, and `command_ack_test` are not leak-checked on macOS today** — all
   four fork subprocesses. Only `obc_sim`/`adcs_sim` are covered (they use `pthread_create`, not
   `fork` — see `rtos/ports/posix/port.c`), via `sim_leak_check_macos`.
6. **ThreadSanitizer (`-fsanitize=thread`) does work on macOS** (verified, compiles and links
   clean) but cannot be combined with ASan in the same binary — `'-fsanitize=address' not allowed
   with '-fsanitize=thread'`. This is a universal sanitizer-runtime rule, not macOS-specific. It
   is **not currently wired into this project's CMake** at all; this codebase has several real
   threads worth race-checking (the CSP router thread, per-connection rx threads, FreeRTOS's
   pthread-per-task model, all touching shared queues/mutexes) — adding a separate
   `ENABLE_TSAN`-style option would be a reasonable future addition, not yet built.
7. **The "not debuggable" warning `leaks` prints is noise, not a blocker**, once
   `MallocStackLogging=1` is set correctly — verified: detection still works correctly with that
   warning present. Codesigning the target with a `com.apple.security.get-task-allow` entitlement
   was tested as a fix for that specific warning and did work, but turned out to be unnecessary on
   this machine once `MallocStackLogging` was set. If a future macOS update makes `leaks`
   genuinely unable to attach, that entitlement is the documented workaround to reach for first.

### Known gap

`comms_bus_test` / `comms_bus_addressing_test` have **no leak-check coverage on macOS today.**
Their real logic runs inside forked "role" processes (master/slave, or 3 addressed nodes) with no
standalone non-forking binary to point a `sim_leak_check_macos.sh`-style check at instead. Closing
this would need either: (a) refactoring these tests so each role can run as its own independently
invocable process, so `leaks` never has to parent something that then forks, or (b) investigating
whether attaching `leaks <pid>` live *after* the fork already happened (rather than `--atExit`,
which supervises the whole run from the start) sidesteps the corruption — neither has been tried.

---

## Windows

Not currently supported, and not treated as a real gap: this project's SIM backend uses POSIX
sockets and pthreads directly (`platform/sim/drivers/comms_i2c.c`, `rtos/ports/posix/port.c`), so
it doesn't build natively on Windows outside WSL — and WSL reports as Linux to CMake
(`CMAKE_SYSTEM_NAME`), so it's already covered by the Linux section above. On native Windows,
`ENABLE_SANITIZERS`/`ENABLE_LEAK_CHECK` print a warning and no-op rather than fail the build. If
native Windows support is ever added for real, the tools to reach for would be Application
Verifier or Dr. Memory — neither is wired up here.

---

## Related files

| File | What it is |
|---|---|
| `CMakeLists.txt` | `ENABLE_SANITIZERS`/`ENABLE_LEAK_CHECK` option definitions, per-OS sanitize-flag selection, the macOS mutual-exclusion guard |
| `tests/CMakeLists.txt` | Per-test wiring; the large comment block there has the full fork-interference finding, in detail |
| `tests/leak_check_macos.sh` | Manual `leaks --atExit` wrapper — safe **only** for a binary that does not call `fork()` |
| `tests/sim_leak_check_macos.sh` | The working `obc_sim`/`adcs_sim` live leak-check, registered as the `sim_leak_check_macos` CTest entry (macOS + `ENABLE_LEAK_CHECK` only) |
