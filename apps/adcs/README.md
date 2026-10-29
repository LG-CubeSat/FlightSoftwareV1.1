# STM32 ADCS application

`Core/Src/main.c` initializes the STM32G431RB HAL, GPIO, USART1, and I2C1,
then calls `adcs_application_start()`. This starts the six existing ADCS tasks
(sensor, estimation, control, command, telemetry, housekeeping), the CSP command
receiver, the CSP router, and the I2C service task under FreeRTOS. The I2C task
replaces the old frame echo loop and also toggles the status LED each second.

The application sources live in `Application/`; both the desktop simulation and
the MCU build compile these sources. `Application/src/sim_main.c`, the simulated
drivers/plant, POSIX FreeRTOS port, and desktop CSP glue are excluded from the MCU.

## Build

From the repository root:

```sh
./firmware/adcs_stm32/build.sh
```

Requires CMake 3.20+, Make, and Arm GNU tools. The script uses a compiler on PATH,
`ARM_TOOLCHAIN_BIN`, or the macOS STM32CubeIDE toolchain. It does not flash.
Outputs: `build-stm32/adcs_stm32.elf`, `.hex`, `.bin`, and `.map`.

Alternatively, import this directory into STM32CubeIDE and build Debug or
Release. Both configurations include Application, Platform, RTOS, and linked
repository kernel/CSP/frame sources. Debug retains debug information but uses
`-Os` to match the memory/stack budget. CubeIDE produces
`firmware/adcs_stm32/Debug/adcs_stm32.elf` (or `Release/`).

The repository root's `HW_MODE=ON` remains the Linux OBC build. It does not build
this firmware; use the standalone firmware project or script above.

## Current behavior and remaining work

- USART1 prints initialization and then ADCS mode, sensor/fault masks, and the
  minimum free stack watermark of the six application tasks once per second.
- I2C1 remains a seven-bit slave at `0x2A`. Existing GET_STATUS, WRITE_FRAME,
  and READ_FRAME envelopes now carry CSP commands, ACK/NACK, and ADCS telemetry.
  The logical CSP address is `ADCS_ADDRESS` from `shared/csp/csp_commands.h`.
  Use the OBC CSP command path; the earlier echo-only bring-up test no longer
  applies. Frames for other destinations or inconsistent CSP sources are dropped.
- The six-packet CSP pool and two-frame transmit queue are bounded. A full
  transmit queue drops the outgoing packet and increments the interface error
  count; it cannot block the scheduler while an OBC is disconnected.
- The STM32 time adapter extends the HAL millisecond tick across rollover and
  accepts OBC Unix-time synchronization. Unix time is unavailable until synced.
  Orbit data is explicitly unavailable until a flight orbit provider is added.
- `Platform/Src/adcs_sensors_unavailable.c` returns NOT_READY for all sensors
  and actuator operations. No IMU, magnetometer, Sun sensor, or rod pinout is
  configured in the current `.ioc`. Replace that file with the actual drivers.
  No simulator runs on the MCU, and no sensor-valid bits are invented.
- Actuators start inhibited. Missing sensor/attitude data keeps ADCS in SAFE.
  `Application/src/estimation/kalman_filter.c` remains unfinished. Booting this
  application does not yet provide detumbling or pointing.
- The independent watchdog starts before the scheduler and housekeeping feeds
  it. Nominal timeout is about 32 seconds (LSI tolerance applies). Assert/stack
  overflow handlers stop feeding it. An intentional shutdown disables interrupts
  and feeds IWDG in the halt loop so shutdown does not turn into a reboot.
- FreeRTOS uses static allocation. The linker reserves 1,536 bytes each for
  the C-library heap and interrupt stack. Task stacks use explicit MCU budgets;
  stack overflow checks and telemetry watermarks remain enabled. Additions to
  the filter or drivers require rechecking stack usage on target.

## Validation

Verified with Arm GCC 14.3.1 through CMake and STM32CubeIDE Debug. The CMake
image uses 28,960 / 32,768 bytes RAM including reserved heap/MSP stack, and
67,296 / 131,072 bytes flash. These are build-time figures, not measured stack
high-water marks.

Host regressions: position commands, ACK/NACK, full constellation telemetry and
payload flow, frame codec, I2C protocol, and the new `stm32_platform_test` for
clock rollover, time sync, and unavailable-device behavior. No physical board
was flashed or exercised during integration. Validate UART output, stable SAFE,
I2C command/telemetry round trips, and all task stack margins on the board next.

## Build maintenance

`RTOS/ARM_CM4F` vendors the unmodified FreeRTOS V11.1.0 GCC Cortex-M4F port,
compatible with the repository's 11.1 kernel API. Its MIT notices are retained.
Source: https://github.com/FreeRTOS/FreeRTOS-Kernel/tree/V11.1.0/portable/GCC/ARM_CM4F

`Platform/Inc/csp/autoconfig.h` is the CubeIDE copy of CMake's generated CSP
configuration. CMake checks equality to prevent drift. Keep the linked sources
in `.project` and source exclusions in `.cproject` in sync with the CMake target.
Both builds compile and link against newlib-nano; the per-task reentrancy and
retargeted locks must use matching C-library headers.

CubeMX regeneration must preserve `adcs_application_start()` in the main user
block and the FreeRTOS includes/SysTick call in the interrupt user blocks.
It may recreate the empty SVC/PendSV handlers: keep their `ADCS_STM32` guards,
because the Cortex-M4F port supplies those symbols. Review the linker reservations
and CubeIDE source entries after regeneration before rebuilding.
