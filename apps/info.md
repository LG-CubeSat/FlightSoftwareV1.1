# STM32 ADCS build and flashing

## Build versus flash versus run

These are four steps across your Mac, the ADCS board, and the Raspberry Pi:

| Action | Computer | Command that does it | Result |
|---|---|---|---|
| **Build ADCS** | Mac | `./apps/adcs/build.sh` | Creates `build-stm32/adcs_stm32.elf` on the Mac; the board is unchanged. |
| **Flash ADCS** | Mac, with ADCS connected by USB DFU or ST-Link | `"$PROGRAMMER" -c port=USB1 -d "$FIRMWARE" -v` for DFU, or `port=SWD` for ST-Link | Writes the built ELF to the STM32 and verifies it. |
| **Build OBC** | Raspberry Pi | `cmake --build build-hw -j4` | Creates Pi executables on the Pi; the STM32 is unchanged. |
| **Run OBC** | Raspberry Pi | `./build-hw/bin/obc_supervisor` | Starts the OBC processes and talks to the already flashed ADCS. |

In particular, `cmake --build build --target obc_supervisor` **only builds an OBC
executable**. It does not flash the ADCS or start the OBC. If that `build` directory
was configured with `HW_MODE=OFF`, it is a simulator executable and cannot use
the physical I2C/radio drivers. See [Build and run the Pi OBC](#4-build-and-run-the-pi-obc)
below.

The ADCS firmware lives in `apps/adcs`:

- `Core`: STM32 startup, HAL initialization, interrupts, UART logging, and the I2C1 slave transport.
- `Drivers`: STM32 HAL and CMSIS sources.
- `Platform`: board-specific adapters, including the BMI270 driver on I2C2.
- `Application`: ADCS tasks, manager, control, estimation, and telemetry.

`apps/adcs/Core/Src/main.c` is the STM32 entry point. `apps/adcs/Application/src/sim_main.c`
is only for the desktop simulator. The root `HW_MODE=ON` build produces the Raspberry Pi OBC,
not the STM32 firmware. See the [ADCS firmware guide](adcs/README.md) for firmware behavior.

## 1. Build the STM32 image on the Mac

Run from anywhere inside the FlightSoftwareV1.1 checkout:

```sh
REPO_ROOT="$(git rev-parse --show-toplevel)"
"$REPO_ROOT/apps/adcs/build.sh"
FIRMWARE="$REPO_ROOT/build-stm32/adcs_stm32.elf"
if test -s "$FIRMWARE"; then ls -lh "$FIRMWARE"; else
  echo "Firmware build did not produce $FIRMWARE" >&2
fi
```

**This is the build step. No firmware has been written to the ADCS yet.** The
script uses the Arm GNU compiler from `PATH`, `ARM_TOOLCHAIN_BIN`, or the bundled
STM32CubeIDE toolchain. It builds `.elf`, `.hex`, and `.bin` files in `build-stm32`; it does
not flash the board. Run it again after source changes so you do not flash an older image.
The `.elf` contains the flash addresses, so no address argument is needed below.

If you specifically need a CubeIDE Debug build instead, use the project name from
`apps/adcs/.project` (`adcs_stm32`):

```sh
CUBEIDE_APP="${CUBEIDE_APP:-/Applications/STM32CubeIDE.app}"
CUBEIDE="$CUBEIDE_APP/Contents/MacOS/STM32CubeIDE"
CUBE_WORKSPACE="$(mktemp -d /tmp/adcs-cubeide.XXXXXX)"
ADCS_PROJECT="$REPO_ROOT/apps/adcs"
"$CUBEIDE" --launcher.suppressErrors -nosplash \
  -application org.eclipse.cdt.managedbuilder.core.headlessbuild \
  -data "$CUBE_WORKSPACE" -import "$ADCS_PROJECT" \
  -cleanBuild 'adcs_stm32/Debug' -no-indexer -printErrorMarkers
FIRMWARE="$ADCS_PROJECT/Debug/adcs_stm32.elf"
if ! test -s "$FIRMWARE"; then echo "CubeIDE did not produce $FIRMWARE" >&2; fi
```

The CMake build above is the simpler default. CubeMX code generation is not a flashing
step; regenerate only when changing hardware configuration, and review generated changes
before building because they can overwrite hand-edited initialization code.

## 2. Locate STM32CubeProgrammer on the Mac

This is one shell command. Keep it on one line; a backslash followed by spaces makes
`find` search for a directory named spaces.

```sh
CUBEIDE_APP="${CUBEIDE_APP:-/Applications/STM32CubeIDE.app}"
PROGRAMMER="$(find "$CUBEIDE_APP" -type f -name STM32_Programmer_CLI -print -quit)"
if ! test -x "$PROGRAMMER"; then
  echo "STM32_Programmer_CLI not found in STM32CubeIDE.app" >&2
fi
printf 'Programmer: %s\nFirmware: %s\n' "$PROGRAMMER" "$FIRMWARE"
```

Use **one** of the connection methods below. A USB cable plugged into an ST-Link is
an SWD connection; it will appear under `-l stlink`, not `-l usb`.

## 3A. Flash the ADCS through its USB DFU connection

Use this only if the ADCS board's USB data lines reach the STM32 USB pins PA11/PA12.
The STM32G431 ROM bootloader supports USB DFU, but the board wiring and BOOT0
configuration still determine whether this path works. See ST's [AN2606 bootloader
note](https://www.st.com/resource/en/application_note/an2606-stm32-microcontroller-system-memory-boot-mode-stmicroelectronics.pdf).

To request the system bootloader using the board buttons: hold **SW2 (BOOT)**, tap
**SW1 (RESET)** while continuing to hold BOOT, then release BOOT. This sequence assumes
those are the actual labels/functions on your board. Connect the board's USB data port
to the Mac, then check for a DFU interface:

```sh
"$PROGRAMMER" -l usb
```

If it lists `USB1`, this is the command that **flashes** the built image to the
STM32 and verifies it:

```sh
"$PROGRAMMER" -c port=USB1 -d "$FIRMWARE" -v
```

Use the port name shown by `-l usb` if it is not `USB1`. After programming, leave
BOOT released and press RESET so the MCU boots the new application from flash.

If `-l usb` says there is no DFU interface, check the data-capable USB cable,
the board USB connector and power, the BOOT/RESET sequence, and whether another
CubeProgrammer instance has the interface open. A normally running application
does not itself appear as the ROM DFU interface.

## 3B. Or flash the ADCS through an ST-Link debug probe

Use this path when an ST-Link is wired to SWDIO, SWCLK, GND, and target power/reference.
The BOOT button is not needed for normal SWD flashing.

```sh
"$PROGRAMMER" -l stlink
"$PROGRAMMER" -c port=SWD -d "$FIRMWARE" -v -rst
```

If the probe cannot connect, first verify board power, common ground, and the
SWD wiring. Do not use `port=USB1` for an ST-Link.

## 4. Build and run the Pi OBC

Do these commands **on the Raspberry Pi**, in its FlightSoftwareV1.1 checkout,
after the ADCS has been flashed and powered. `HW_MODE=ON` selects the physical
Pi I2C and E22 drivers:

```sh
cmake -S . -B build-hw -DHW_MODE=ON -DBUILD_TESTS=OFF
cmake --build build-hw -j4
```

That is the **OBC build**. It neither flashes the ADCS nor starts the OBC.
Start the built OBC with:

```sh
BENCH_DIR="$(mktemp -d)"
MISSION_STATE_PATH="$BENCH_DIR/mission.state" \
  OBC_TELEMETRY_LOG_PATH="$BENCH_DIR/telemetry.bin" \
  MISSION_TELEMETRY_DOWNLINK_INTERVAL_SEC=1 \
  MISSION_ASCENT_WAIT_SEC=100000 MISSION_AUTONOMY_ENABLED=0 \
  ./build-hw/bin/obc_supervisor
```

The terminal stays open while the OBC runs. Do not start `adcs_sim` when using
the real ADCS board. If you already built `./build/bin/obc_supervisor`, check
`grep '^HW_MODE:BOOL=' build/CMakeCache.txt` on the Pi. You may run that binary
instead only if it says `HW_MODE:BOOL=ON`.

## 5. Confirm the firmware and connection run

After the reset, check the ADCS USART1 diagnostic output and then confirm the Pi
sees the board at I2C address `0x2A`. Finally run the OBC and ground station and
look for a changing ADCS telemetry sequence and a command ACK. Successful flash
verification proves the bytes were written; it does not prove the application,
BMI270, or Pi-to-ADCS link is healthy.
