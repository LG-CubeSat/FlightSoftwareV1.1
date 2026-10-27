# STM32 ADCS source layout

- `firmware/adcs_stm32/Core`: CubeMX startup, HAL initialization, interrupts,
  UART logging, and I2C slave transport.
- `firmware/adcs_stm32/Drivers`: vendor HAL and CMSIS sources.
- `firmware/adcs_stm32/Application`: the relocated ADCS tasks, manager,
  controllers, estimator, telemetry, and desktop simulation.

The simulation entry point is `Application/src/sim_main.c`; the MCU entry
point is `Core/Src/main.c`, which now starts the ADCS FreeRTOS application.
Build with `./firmware/adcs_stm32/build.sh` or CubeIDE. See
[the firmware integration guide](adcs_stm32/README.md) for current capabilities
and the missing sensor, actuator, orbit, and estimator implementations.

# Flashing
So the two buttons on the ADCS board are like Boot and Reset. To get it like flashable, hold SW2 (Boot) and then SW1 (Reset) click while still holding. Then let go of SW2 (Boot).

Normally fidling with these buttons in some way gets it to work..

# Checking connections:

*Set up the progammer*
PROGRAMMER="$(find /Applications/STM32CubeIDE.app \   
  -type f -name STM32_Programmer_CLI | head -1)"

*Run search*
"$PROGRAMMER" -l usb

**NOTE: Sometimes it wil say no DFU interface because the board is actively running or the CubeProgrammer is latched on

# Deployment
So use the CubeMX to configure pins, etc. Generate the code to the correct folder in this repo.
Rebuild the code using:

```
CUBEIDE="/Applications/STM32CubeIDE.app/Contents/MacOS/STM32CubeIDE"
CUBE_WORKSPACE="/tmp/flightsoftware-cubeide-workspace"
ADCS_PROJECT="$PWD/apps/adcs"

mkdir -p "$CUBE_WORKSPACE"

"$CUBEIDE" \
  --launcher.suppressErrors \
  -nosplash \
  -application org.eclipse.cdt.managedbuilder.core.headlessbuild \
  -data "$CUBE_WORKSPACE" \
  -import "$ADCS_PROJECT" \
  -cleanBuild "adcs/Debug" \
  -no-indexer \
  -printErrorMarkers
```

*You want to see "Build Finnished. 0 erorrs, 0 warnings..."*

Confirm the output via:
```
FIRMWARE="$ADCS_PROJECT/Debug/adcs_stm32.elf"

ls -lh "$FIRMWARE"
```
A .elf file should exist

Enter DFU mode:
```
PROGRAMMER="$(find /Applications/STM32CubeIDE.app \
  -type f -name STM32_Programmer_CLI | head -1)"

"$PROGRAMMER" -l usb
```
You may need to do button reset. 

# Then flash it on using:
```
FIRMWARE="$PWD/apps/adcs/Debug/adcs_stm32.elf"

test -f "$FIRMWARE" && echo "Firmware found"

"$PROGRAMMER" \
  -c port=USB1 \
  -d "$FIRMWARE" \
  -v
```