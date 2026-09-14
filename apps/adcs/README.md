# ADCS simulation

This directory contains a working software-in-the-loop ADCS starting point. It
models a CubeSat in orbit, exposes hardware-shaped sensor reads, estimates the
current attitude, stores the current and target attitudes in one manager-owned
context, selects a mode, calculates control with PID/B-dot controllers, and
feeds actuator commands back into the simulated rigid body.

It is an engineering simulation, not flight-qualified control software. The
models and gains are deliberately small and understandable so real drivers and
better environmental models can replace them later.

## Build and run

From the repository root:

```sh
cmake -S . -B build -DHW_MODE=OFF
cmake --build build --target adcs_sim
build/apps/adcs/adcs_sim --standalone
```

`--standalone` runs all FreeRTOS tasks without requiring an OBC/CSP peer.
Running without an argument enables CSP command and telemetry transport and
waits for the normal repository network setup.

## The whole data flow

```text
simulated orbit + rigid-body truth
              |
              v
 IMU / magnetometer / Sun / thermistor reads       command queue
              |                                          |
              v                                          v
        sensor packet ---> reference vectors ---> ADCS manager context
              |                    |                | current attitude
              +--> TRIAD seed + multiplicative -----+ target attitude
                   attitude filter                  | active mode
                                                   v
                                   B-dot / Sun acquisition / quaternion PID
                                                   |
                              +--------------------+--------------------+
                              v                                         v
                       magnetorquers                           reaction wheels
                              +--------------------+--------------------+
                                                   |
                                                   v
                                           simulated rigid body
```

The manager context is the source of truth shared across tasks. Sensor and
estimation tasks update it; control takes one coherent snapshot from it; mode
logic updates the built-in target; telemetry takes another coherent snapshot.
The mutex in `adcs_manager.c` prevents readers from seeing half-updated state.

Task rates are intentionally conventional for this simulation:

- Sensor and plant step: 100 Hz
- Estimation: once per sensor packet, up to 100 Hz
- Control and mode evaluation: 20 Hz
- Telemetry: 5 Hz
- Fault/health evaluation: 1 Hz

## Quaternion and frame convention

All attitudes use cglm's `versor`, whose storage order is `[x, y, z, w]`.
The quaternion rotates an ECI vector into the spacecraft body frame. Current
attitude lives in `latest_attitude.quaternion`; commanded attitude lives in
`guidance_target.target_quaternion`.

This convention matters when forming the physical attitude error. The control
math uses the shortest rotation from current attitude to target attitude and
converts that error into a body-rate target before the three axis PID loops.
Quaternions are normalized at subsystem boundaries, and `q` and `-q` are
treated as the same attitude.

## Modes

The implemented mode set is:

- `BOOT`: wait for valid, fresh sensors and a valid attitude estimate.
- `SAFE`: actuators off while faults or prerequisites prevent control.
- `DETUMBLE`: B-dot magnetic damping until body rate remains below threshold.
- `SUN_ACQUISITION`: turn the configured body Sun axis toward the Sun with
  magnetorquers.
- `SUN_POINTING`: maintain the Sun target with reaction-wheel control.
- `EARTH_POINTING`: continuously regenerate a nadir target and hold it.
- `SLEWING`: move toward a commanded cglm attitude or vector target.
- `TARGET_POINTING`: hold the target after the slew settles.
- `SCIENCE`: hold the commanded target using the precise controller.

Nominal startup requests Sun pointing and follows
`BOOT -> SAFE -> DETUMBLE -> SUN_ACQUISITION -> SUN_POINTING`. The detumble
step is used only while rate is above its entry threshold. A critical sensor,
attitude, rate, temperature, or actuator fault forces `SAFE`.

## Estimation and guidance

The simulator supplies a gyro, accelerometer, magnetometer, coarse Sun vector,
irradiance, and board temperature through separate read functions. No function
uses `_mock` in its name: these functions are the simulation implementation of
the hardware-shaped boundary.

Reference generation provides low-order Sun, magnetic-field, and nadir ECI
vectors from simulation time and orbit position. Two non-collinear measured
vectors seed attitude with TRIAD. A lightweight multiplicative filter then
propagates with gyro rate and applies bounded Sun/magnetic vector corrections.
It carries an explicit gyro-bias state, seeded at zero in this MVP, and tracks
estimate confidence. This is sufficient for the software-in-the-loop starting
point, but it is not a full flight MEKF or an on-orbit gyro-bias calibration.

Built-in Sun and Earth modes turn an inertial direction plus a body pointing
axis into an ECI-to-body target `versor`. Explicit commands can instead supply
a target `versor` or an inertial direction/body-axis pair.

## Control and actuators

`DETUMBLE` uses B-dot control and magnetorquers. `SUN_ACQUISITION` calculates a
Sun-pointing torque and projects the achievable part through
`torque = dipole x magnetic_field`. Magnetic control cannot produce torque
parallel to the local field, which is why it is not used for arbitrary precise
three-axis holding.

Sun hold, Earth hold, commanded slew, target hold, and science modes use a
quaternion-to-rate outer loop and three rate PID controllers. The result is a
three-axis reaction-wheel body-torque command. Integral anti-windup, rate and
torque limits, shortest-path quaternion errors, settle thresholds, wheel torque
limits, and wheel momentum limits are represented.

The simulator integrates diagonal-inertia Euler rigid-body dynamics. It also
advances a circular inclined orbit, calculates simple Sun and tilted-dipole
magnetic references, adds deterministic sensor bias/noise, and supports sensor
and actuator fault injection. The model is deterministic so simulation runs
are repeatable.

## File map

- `include/communication/message.h`: all task messages, modes, current/target
  state, actuator outputs, health, and command types.
- `src/simulation/`: sensor/actuator boundary plus orbit and rigid-body plant.
- `src/estimation/`: reference vectors, TRIAD initialization, gyro propagation,
  and vector corrections.
- `src/guidance/`: attitude and vector target generation.
- `src/control/`: reusable math, B-dot, Sun pointing, quaternion PID, magnetic
  allocation, and mode dispatcher.
- `src/manager/`: canonical state/context, transition logic, health, faults,
  and watchdog behavior.
- `src/tasks/`: periodic acquisition, estimation, control, telemetry,
  housekeeping, and command execution.
- `src/communication/`: CSP command decoding and explicit big-endian telemetry
  encoding.
- `src/main.c`: standalone and CSP-connected startup paths.

## Command and telemetry boundary

The existing repository commands for position, reset, shutdown, Sun pointing,
and time sync remain supported. ADCS-local command IDs add mode selection,
target attitude, target vector, estimator reset, actuator inhibit, and simulator
fault injection. The legacy integer position command maps degrees about body Z
to a real target quaternion so existing integration tests still exercise the
new controller path.

Telemetry format version 1 starts with `ADCS`, then carries mode, sequence,
timestamp, validity/fault masks, current and target quaternions, rates, bias,
sensors, requested/achievable control, actuator flags, and health counters in
explicit big-endian form. Native C struct padding is never transmitted by the
telemetry encoder.

## Before connecting hardware

The simulation is a reasonable interface and control starting point. Hardware
work should replace the simulator read/write calls behind drivers without
changing the manager messages. Before flight or high-fidelity HIL testing:

- Measure inertia, sensor alignment, actuator polarity, wheel limits, noise,
  bias stability, and timing; then retune every control/estimator parameter.
- Replace the circular orbit, low-order Sun model, and dipole magnetic model
  with mission time/orbit services and validated reference models.
- Add eclipse handling, magnetometer quiet sampling around rod actuation,
  reaction-wheel momentum unloading, and wheel-fault handling.
- Decide whether the mission accuracy requires a full MEKF and higher-order
  environment/actuator dynamics.
- Define and version an explicitly encoded command wire format, as telemetry
  already does, instead of depending on native command-struct layout.
- Run unit, software-in-the-loop, processor-in-the-loop, HIL, fault-injection,
  Monte Carlo, and mission-scenario tests before treating outputs as safe.
