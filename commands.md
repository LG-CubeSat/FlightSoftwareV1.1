# Run the main OBC code
'''
sudo systemctl stop obc.service
sudo systemctl start pigpiod
pigs m 25 r
pigs pud 25 o
cmake -S . -B build-hw -DHW_MODE=ON -DBUILD_TESTS=ON
cmake --build build-hw --target obc_supervisor -j4
E22_SWAPPED_UART=1 ./build-hw/bin/obc_supervisor
'''

# Build the ADCS Code
'''
CUBEIDE_APP="${CUBEIDE_APP:-/Applications/STM32CubeIDE.app}"                                                                        
PROGRAMMER="$(find "$CUBEIDE_APP" -type f -name STM32_Programmer_CLI -print -quit)"
if ! test -x "$PROGRAMMER"; then
  echo "STM32_Programmer_CLI not found in STM32CubeIDE.app" >&2
fi
printf 'Programmer: %s\nFirmware: %s\n' "$PROGRAMMER" "$FIRMWARE"
'''

# Flash the ADCS Code
'''
/Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/Resources/bin/STM32_Programmer_CLI \
  -c port=USB1 -d apps/adcs/Debug/adcs_stm32.elf -v -s
'''