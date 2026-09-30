# Flight Readiness TODO

Status snapshot from a full read-through of `apps/obc/`, `platform/`, `shared/`, and the build
system, plus empirical checks (SIM build, a live crash-recovery test, static HW_MODE link
analysis). Organized by how badly each item blocks "runs cleanly and indefinitely on the Pi."

Docker was unavailable when this was written, so the HW_MODE findings below are from static
analysis (grepping definitions vs. call sites, reading `platform/CMakeLists.txt`'s branches),
not a real cross-compiled link. Verify 0.1/0.3 with an actual HW_MODE build once Docker's back.

---

## Tier 0 — HW_MODE doesn't build or run

- [X] **0.1 — Add an honest-stub HW radio abstraction.**
      `platform/real/drivers/radio.c` implements the shared send/receive interface as explicit
      failure stubs and is part of the HW `platform` target, so mission links without pretending
      real radio traffic works. The E22 implementation is deliberately deferred to the teammate
      working on radio.
- [X] **0.2 — Give `radio.h` a receive path.** The shared API and HW stub now have
      `radio_receive`. The ground-uplink listener and real E22 behavior remain deferred with the
      rest of the radio work; current OBC testing is downlink-path only.
- [X] **0.3 — Stop building ADCS/EPS in the Pi (HW_MODE) build.** Root `CMakeLists.txt` adds
      `apps/adcs` and `apps/eps` unconditionally. Under `HW_MODE=ON` on a Pi these would link
      the Linux-only real I2C driver and the POSIX FreeRTOS port — nonsense for an STM32, and it
      currently just compiles and hides the problem. Gate both behind `if (NOT HW_MODE)` until
      the ARM cross-compilation split happens (see `docs/satellite_architecture.md` §4 item B.3).
      *(Full walkthrough below.)*
- [X] **0.4 — Real camera driver is a stub.** `platform/real/drivers/camera.c` returns -1
      unconditionally. In HW mode the mission timeline dies at its first step. Confirmed
      hardware: Arducam OV5647, CSI ribbon (not USB-C, doc corrected) — driven through
      `libcamera`/`rpicam-apps`, not V4L2 UVC. Implementation: shell out to `rpicam-jpeg` via
      `posix_spawn` (writes real JPEG directly, no separate encode step needed for SSDV). *(In
      progress.)*
- [X] **0.5 — Add HW_MODE-safe automated coverage.** `frame_codec_test`, `obc_ipc_test`,
      `ssdv_roundtrip_test`, and `compute_async_test` build and run with `HW_MODE=ON` without
      touching the camera, radio, or I2C device. SIM-only constellation/bus tests remain gated
      behind `if (NOT HW_MODE)`. See `docs/testing.md` for the test and CMake walkthrough.

## Tier 1 — bugs that break indefinite operation

- [X] **1.1 — Supervisor never restarts a crashed process.** Verified live: SIGKILL'd
      `obc_time`, supervisor logged `time killed by signal 9`, and 8s later it was still dead.
      `supervisor_reap` (`apps/obc/supervisor/src/processes.c:125`) sets `pid = -1` with the
      comment "ready to restart with backoff" — but nothing ever calls a restart for it.
      `supervisor_check_frozen` explicitly skips `pid <= 0` slots. This is the single biggest
      blocker to "runs indefinitely": today the OBC only recovers from a *hang*, never a
      *crash*. Fix: in `supervisor_heartbeat`, after reaping, respawn any slot with
      `pid == -1`, with backoff + a restart-count ceiling so a crash-loop doesn't fork-bomb.
- [X] **1.2 — Mission is one-shot and ignores every failure.** `scheduler.c:63-85` runs
      ascent → photo → compress → downlink → `MISSION_DONE`, then spins forever doing nothing.
      Worse, it ignores return codes: if `payload_commander_take_photo` fails, the state
      machine advances to `MISSION_COMPRESSING` anyway and compresses a file that doesn't
      exist. Needs a repeating capture/downlink cadence and per-step retry/failure handling.
- [X] **1.3 — Mission state doesn't survive a restart.** `scheduler_thread` takes
      `clock_gettime` at thread start, so any supervisor restart (including the fix in 1.1)
      resets the ascent timer to zero and replays the whole timeline from scratch. Persist
      mission phase to disk and reload on start. Also note `CLOCK_MONOTONIC` itself resets at
      Pi reboot.
- [ ] **1.4 — `IPC_receive` has no timeout, and this wedges `compute` permanently.** If `data`
      dies mid-stream, the worker thread blocks forever in `wait_for_reply` with `job_busy = 1`
      held — every subsequent compress request gets `COMPUTE_STATUS_BUSY` for the rest of the
      flight. Add a receive timeout and a job deadline that releases `job_busy`.
- [ ] **1.5 — Heartbeat proves liveness, not progress.** Each process's `heartbeat_thread`
      pings supervisor on a fixed 1s timer independent of whether its actual worker threads are
      making progress. A `compute` wedged per 1.4 keeps heartbeating happily forever. Make the
      heartbeat reflect real work (e.g. worker threads bump a counter; heartbeat only pings if
      the counter moved).
- [ ] **1.6 — Buffer overflow in compute's worker.** `worker.c:81`:
      `memcpy(input_buf + input_len, reply.payload, reply.length)` has no bound check against
      `COMPUTE_MAX_DATA_SIZE` (64KB). `payload_commander.c:92` checks this on the sending side,
      the worker doesn't on the receiving side. A real Arducam JPEG over 64KB triggers this on
      the very first photo. Add the bound check; consider raising or streaming past the cap.
- [ ] **1.7 — Out-of-bounds write in supervisor.** `supervisor.c:121` calls
      `supervisor_mark_alive(src)` where `src` comes straight off the wire
      (`obc_ipc.c` reads it unchecked from the header byte), indexing
      `last_heartbeat[ROLE_TIME + 1]` — an 8-element array. Any `src > 7` from a malformed or
      malicious message corrupts adjacent memory. Range-check `src` before indexing.
- [ ] **1.8 — FDIR is mostly inert.** `fallback_handle_fault` is dead code — nothing calls it.
      `watchdog.c` is just a heartbeat ticker, not a real watchdog. The only live FDIR path is
      `health_monitor` reacting to board reset notices. Either wire FDIR into real OBC-role
      fault detection, or accept that supervisor's heartbeat/restart loop is the entire
      recovery story (and make sure 1.1/1.5 are solid if so).
- [ ] **1.9 — ADCS telemetry is received and discarded.** `ingest.c` routes `ADCS_TELEM_PORT`
      to `ROLE_MISSION`, but nothing in `mission` ever reads/stores/limit-checks it — the
      transient `IPC_receive` loops inside `payload_commander` just skip wrong-sized messages.
      No telemetry is stored or downlinked anywhere. Biggest remaining *feature* gap: for a
      flight where radio is the only way to learn what happened, there is currently no
      telemetry pipeline at all.

## Tier 2 — Pi ops: boot to recovery

- [ ] **2.1 — Nothing starts `obc_supervisor` on boot.** Write a systemd unit
      (`Restart=always`, sane `RestartSec`, `After=network.target`).
- [ ] **2.2 — No hardware watchdog.** If `obc_supervisor` itself dies or the kernel wedges,
      nothing recovers. Enable `bcm2835_wdt`, set `RuntimeWatchdogSec` in
      `/etc/systemd/system.conf`, have supervisor `sd_notify` its liveness.
- [ ] **2.3 — All state lives under `/tmp`, which is tmpfs on Raspberry Pi OS.** Photos,
      compressed output, and every IPC socket vanish on reboot (`scheduler.c`'s `PHOTO_PATH`/
      `COMPRESSED_PHOTO_PATH`, `obc_ipc.c`'s socket paths). Move payload data to
      `/var/lib/obc`, sockets to `/run/obc`.
- [ ] **2.4 — No disk-space management.** Indefinite operation with periodic capture fills the
      SD card and every subsequent write fails silently. Add retention/rotation and a
      free-space check before capture.
- [ ] **2.5 — Writes aren't power-loss safe.** `filesystem_write_chunk` does
      `fopen`/`fwrite`/`fclose` per chunk with no `fsync`. A brownout mid-write corrupts the
      file. `fsync` at minimum on the final chunk; consider write-to-temp-then-rename.
- [ ] **2.6 — No persistent log.** Everything is `printf` to stdout. Under systemd this lands
      in journald — configure persistent journald storage or there's no post-flight forensics.
      Also `setvbuf(stdout, NULL, _IOLBF, 0)` in each `main` instead of relying on scattered
      manual `fflush` calls.

## Tier 3 — real drivers and the I2C protocol

- [ ] **3.1 — Real I2C receive design needs rework.** `comms_bus_receive`
      (`platform/real/drivers/comms_i2c.c`) loops forever internally and can never report
      "bus idle" vs. "bus dead." It also does a fixed 132-byte read from every slave every
      20ms regardless of whether they have data — wasteful at 100kHz with multiple slaves.
      Add a real timeout return and a lightweight "do you have data" poll before the full read.
- [ ] **3.2 — Slave-side "no data" contract is unspecified.** The OBC treats
      `frame.length == 0` as "checked in, nothing new," which requires the STM32 firmware to
      deliberately format an empty frame on request. An unprepared slave clocking out `0xFF`
      deserializes to `length = 0xFFFF` and logs a malformed-frame error every 20ms — a log
      flood. Document this contract explicitly before ADCS/Thermals firmware is written.
- [ ] **3.3 — Unconfirmed hardware facts blocking real driver work:** I2C slave addresses are
      placeholders (`platform/real/include/i2c_addresses.h`, `0x42`/`0x43`), E22 module
      variant/framing is unknown, exact Arducam model/interface is unknown. See
      `docs/satellite_architecture.md` §5 Open Items.

---

## Suggested order

1. Tier 0 first — 0.1 and 0.3 are small and unblock everyone else working on the Pi build.
2. Then 1.1 (process restart) and 1.4/1.5 (real wedge detection) — this is what "runs
   indefinitely" actually means in practice.
3. Then 2.1–2.3 so the system survives a power cycle / reboot.
4. 1.9 (telemetry pipeline) is the biggest remaining feature gap and worth scheduling
   deliberately rather than bolting on late.

## Doc correction needed

`docs/satellite_architecture.md`'s bring-up snapshot is older than the current implementation:
the I2C signatures were repaired, the camera backend was implemented, the honest radio stub is
now linked, and portable HW_MODE tests exist. The complete hardware build still needs to be
verified on the Raspberry Pi because non-Linux hosts do not provide `<linux/i2c-dev.h>`.
