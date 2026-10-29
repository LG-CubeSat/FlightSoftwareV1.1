set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
# Set ARM_TOOLCHAIN_BIN to the directory containing arm-none-eabi-gcc if it is
# not on PATH (for example the GNU tools bundled with STM32CubeIDE).
find_program(CMAKE_C_COMPILER arm-none-eabi-gcc HINTS "$ENV{ARM_TOOLCHAIN_BIN}" REQUIRED)
find_program(CMAKE_ASM_COMPILER arm-none-eabi-gcc HINTS "$ENV{ARM_TOOLCHAIN_BIN}" REQUIRED)
find_program(CMAKE_OBJCOPY arm-none-eabi-objcopy HINTS "$ENV{ARM_TOOLCHAIN_BIN}" REQUIRED)
find_program(CMAKE_SIZE arm-none-eabi-size HINTS "$ENV{ARM_TOOLCHAIN_BIN}" REQUIRED)
set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")
set(CMAKE_ASM_FLAGS_INIT "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")
