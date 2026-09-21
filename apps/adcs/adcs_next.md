# ADCS next steps

Status snapshot as of this branch: the simulation builds clean (`-Wall -Wextra
-Wpedantic`, zero warnings from ADCS code), integrates cleanly with `main`,
and the task/manager/control/telemetry/fault layers are in solid shape. The
one thing standing between this and a working closed loop is the estimator.
This file tracks what's left, roughly in the order it blocks everything
after it.

## 1. Implement the attitude filter math (blocking everything else)

`apps/adcs/src/estimation/kalman_filter.c` is the last unimplemented piece.
Every function in it is currently a stub that does nothing and returns
`ADCS_RESULT_NOT_INITIALIZED`:

- `adcs_kalman_filter_init`
- `adcs_kalman_filter_reset`
- `adcs_kalman_filter_predict`
- `adcs_kalman_filter_correct_vector`
- `adcs_kalman_filter_get_state`

Because `adcs_attitude_estimator_init()` sets `estimator->initialized` from
`filter.initialized` (currently always 0, since `init` just zeroes the
struct), `adcs_attitude_estimator_update()` rejects every call at its first
guard. No sensor data ever reaches a valid attitude, `attitude_ready_locked()`
in `adcs_manager.c` never returns true, and the spacecraft is permanently
stuck in `ADCS_MODE_BOOT` no matter how good the simulator, controller, or
telemetry are. Everything downstream of the estimator is otherwise ready and
waiting on this.

The interface contract is already fully specified in
`apps/adcs/include/estimation/kalman_filter.h` — config fields (gyro noise,
bias-walk, initial variances, measurement floor, innovation gate) and a
docstring per function describing exactly what each one needs to do
(6-state covariance propagation, bounded complementary vector correction,
etc.). `attitude_estimator.c` already does the surrounding work correctly:
TRIAD seeding, sample-gap handling, per-sensor correction gating, rejection
counting, and corrupt-state detection (`filter_state_is_corrupt()` in the
same file) that feeds `fault_management_check_estimator_bounds()` for an
automatic reset. None of that needs to change — it's just waiting on a real
filter behind it.

## 2. Tune the controller against the (now working) closed loop

The commit history already flags the PID gains as provisional
(`controller_config()` in `control_task.c`). Once the filter produces real
attitude estimates, run the sim end-to-end and retune the B-dot,
sun-pointing, and slew PID gains against actual closed-loop behavior —
right now they were necessarily tuned blind, since the loop was never
closed.

## 3. Add automated test coverage

There are currently no unit or integration tests for any ADCS logic.
`tests/test_comms_bus*.c` only exercise CSP transport/addressing, not the
estimator, controller, or fault manager. Once the filter exists, at minimum:

- Unit tests for `kalman_filter.c` (predict/correct against known
  trajectories, covariance sanity, innovation gating).
- A closed-loop sim test asserting the mode sequence
  `BOOT -> SAFE -> DETUMBLE -> SUN_ACQUISITION -> SUN_POINTING` actually
  completes within a bounded sim time.
- Fault-injection tests exercising the fault mask / SAFE fallback / reset
  paths in `fault_manager.c`.

## 4. Everything already tracked in the README

`apps/adcs/README.md`'s "Before connecting hardware" section already covers
the hardware-facing follow-up work in detail (real driver implementations
behind `shared/interfaces/*.h`, sensor/actuator characterization, replacing
the low-order orbit/Sun/magnetic models, command protocol versioning, and
the full HIL/Monte Carlo/mission-scenario test matrix). That list is still
accurate and doesn't need duplicating here — treat it as the follow-on to
items 1-3 above, not an alternative to them.

## Open question: should CMD_MOVE_TO_POSITION reject out-of-range commands?

`command_handler.c`'s `CMD_MOVE_TO_POSITION` case no longer bounds-checks
`target_position` at all (the old `fault_management_check_bounds()` /
`POSITION_LIMIT` mechanism was removed). This looks like a deliberate
consequence of moving bounds-checking off the command path and onto the
estimator (`fault_management_check_estimator_bounds()`, called from
`estimation_task.c`), matching the design comment that used to sit on this
exact case ("bounds-checking a commanded value here would be wrong...
checking our own computed/estimated state is estimation_task's job") — and
in practice a large/garbled value is harmless today since
`dispatch_legacy_position()` runs it through `fmodf(..., 360.0F)` before
turning it into a quaternion, so it can't produce an invalid attitude
target. But it does mean a garbled `CMD_MOVE_TO_POSITION` payload is now
silently accepted and slewed to (wrapped into some angle) instead of
rejected/flagged the way it used to be. Worth a deliberate yes/no rather
than leaving it as a side effect of the estimator-bounds rework — not
changed here since it's a policy call, not a bug.

## Smaller things worth a look (not changed here, none are blocking)

- `filter_state_is_corrupt()`/`fault_management_check_estimator_bounds()`
  reject a quaternion only when its squared norm leaves roughly [0.5, 1.5],
  while `fault_management_evaluate_adcs()` flags `ADCS_FAULT_ATTITUDE_INVALID`
  at a much tighter |norm - 1| > 0.05. Worth confirming that gap between the
  "is this corrupted" gate and the "is this faulted" gate is intentional
  (they do serve different purposes: one triggers a reset, the other a
  telemetry fault) rather than two copies that were meant to match.
- `adcs_slew_update()` (`slew_controller.c`) clamps the combined 3-axis
  torque vector to `maximum_torque_nm` after each axis's PID output was
  already independently clamped to the same magnitude
  (`control_task.c`'s `output_limit` and `maximum_torque_nm` are both
  `0.000008F`). When multiple axes saturate at once the combined-vector
  clamp can attenuate all three below their individually-tuned limit. Once
  the estimator is in and there's a real closed loop to tune against (item
  2 above), worth deciding if that's the intended interaction.
- The "critical fault" mask was duplicated between `adcs_manager.c` and
  `housekeeping_task.c` — centralized into `ADCS_FAULT_ALL` in
  `fault_manager.h` as part of this cleanup pass, and `state_is_fresh_locked()`
  in `adcs_manager.c` had a self-subtraction bug (`now` was set to
  `state.latest_sensors.timestamp_us` and then subtracted from itself,
  making the sensor-freshness half of the check permanently vacuous) — fixed
  by threading a real `now_us` into `adcs_manager_update()` the same way
  `housekeeping_task.c` already does. Both are already applied on this
  branch, listed here only for the record.
- `adcs_kalman_filter_correct_vector()`'s documented contract takes unit
  vectors, but `attitude_estimator.c` currently passes the raw
  `magnetic_field_t` reading (in tesla, not normalized) straight through.
  Harmless today since the filter is a stub, but get the normalization
  right (whichever side of the call it belongs on) as part of item 1.
- `refresh_builtin_target_locked()` in `adcs_manager.c` hardcodes the Sun
  body-pointing axis as `{1,0,0}` independently of
  `adcs_sun_pointing_config_t.pointing_axis_body` (configured separately in
  `control_task.c`) and of a third `{1,0,0}` literal in
  `adcs_manager_init()`. All three agree today by coincidence; worth a
  single source of truth before the pointing axis ever changes.

## Housekeeping (non-blocking, do whenever convenient)

- `apps/adcs/include/tasks/example.md` and `building_a_task.md` are
  developer scratch/reference notes living inside a header directory (not
  referenced by any code, harmless to the build). Worth moving out of
  `include/` into a `docs/` location at some point so the header directory
  only contains headers, but not urgent.
