cmake_minimum_required(VERSION 3.20)
project(adcs_stm32 C ASM)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Generic")
    message(FATAL_ERROR "Use -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake")
endif()
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(CMAKE_BUILD_TYPE MinSizeRel CACHE STRING "Build type")
add_compile_definitions(ADCS_STM32 USE_HAL_DRIVER STM32G431xx)
add_compile_options($<$<COMPILE_LANGUAGE:C>:--specs=nano.specs> -ffunction-sections -fdata-sections -fstack-usage -Wall -Wextra)
add_library(freertos_kernel STATIC
    ${REPO_ROOT}/rtos/kernels/src/tasks.c
    ${REPO_ROOT}/rtos/kernels/src/list.c
    ${REPO_ROOT}/rtos/kernels/src/queue.c
    RTOS/ARM_CM4F/port.c)
target_include_directories(freertos_kernel PUBLIC
    RTOS RTOS/ARM_CM4F ${REPO_ROOT}/rtos/kernels/include)
set(CSP_SYSTEM_NAME freertos)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CSP_USE_RDP OFF CACHE BOOL "" FORCE)
set(CSP_USE_HMAC OFF CACHE BOOL "" FORCE)
set(CSP_USE_PROMISC OFF CACHE BOOL "" FORCE)
set(CSP_BUFFER_COUNT 6 CACHE STRING "" FORCE)
set(CSP_CONN_MAX 4 CACHE STRING "" FORCE)
set(CSP_CONN_RXQUEUE_LEN 4 CACHE STRING "" FORCE)
set(CSP_QFIFO_LEN 8 CACHE STRING "" FORCE)
set(CSP_PORT_MAX_BIND 31 CACHE STRING "" FORCE)
add_subdirectory(${REPO_ROOT}/libs/libcsp ${CMAKE_CURRENT_BINARY_DIR}/libcsp)

target_include_directories(csp PRIVATE Platform/Inc)
# CubeIDE uses a checked-in copy; reject drift between the two build paths.
file(READ "${CMAKE_CURRENT_BINARY_DIR}/libcsp/include/csp/autoconfig.h" CSP_GENERATED_CONFIG)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/Platform/Inc/csp/autoconfig.h" CSP_CUBE_CONFIG)
if(NOT CSP_GENERATED_CONFIG STREQUAL CSP_CUBE_CONFIG)
    message(FATAL_ERROR "Update Platform/Inc/csp/autoconfig.h to match the generated libcsp configuration")
endif()

set(HAL_DIR Drivers/STM32G4xx_HAL_Driver)
add_executable(adcs_stm32
    Core/Src/main.c Core/Src/stm32g4xx_it.c Core/Src/stm32g4xx_hal_msp.c
    Core/Src/system_stm32g4xx.c Core/Src/syscalls.c Core/Src/sysmem.c
    Core/Src/adcs_i2c_slave.c Core/Src/adcs_log.c
    Core/Startup/startup_stm32g431rbtx.s
    ${HAL_DIR}/Src/stm32g4xx_hal.c
    ${HAL_DIR}/Src/stm32g4xx_hal_cortex.c
    ${HAL_DIR}/Src/stm32g4xx_hal_dma.c
    ${HAL_DIR}/Src/stm32g4xx_hal_dma_ex.c
    ${HAL_DIR}/Src/stm32g4xx_hal_gpio.c
    ${HAL_DIR}/Src/stm32g4xx_hal_flash.c
    ${HAL_DIR}/Src/stm32g4xx_hal_flash_ex.c
    ${HAL_DIR}/Src/stm32g4xx_hal_flash_ramfunc.c
    ${HAL_DIR}/Src/stm32g4xx_hal_pwr.c
    ${HAL_DIR}/Src/stm32g4xx_hal_pwr_ex.c
    ${HAL_DIR}/Src/stm32g4xx_hal_rcc.c
    ${HAL_DIR}/Src/stm32g4xx_hal_rcc_ex.c
    ${HAL_DIR}/Src/stm32g4xx_hal_exti.c
    ${HAL_DIR}/Src/stm32g4xx_hal_i2c.c
    ${HAL_DIR}/Src/stm32g4xx_hal_i2c_ex.c
    ${HAL_DIR}/Src/stm32g4xx_hal_uart.c
    ${HAL_DIR}/Src/stm32g4xx_hal_uart_ex.c
    Platform/Src/adcs_application.c Platform/Src/adcs_board.c
    Platform/Src/adcs_newlib_locks.c
    Platform/Src/adcs_csp_stm32.c Platform/Src/adcs_environment_stm32.c
    Platform/Src/adcs_sensors_unavailable.c
    ${REPO_ROOT}/shared/interfaces/frame.c
    Application/src/freertos_hooks.c
    Application/src/manager/adcs_manager.c Application/src/manager/fault_manager.c
    Application/src/tasks/command_task.c Application/src/tasks/sensor_task.c
    Application/src/tasks/control_task.c Application/src/tasks/estimation_task.c
    Application/src/tasks/housekeeping_task.c Application/src/tasks/telemetry_task.c
    Application/src/communication/command_handler.c Application/src/communication/telemetry.c
    Application/src/control/actuator_allocator.c Application/src/control/bdot.c
    Application/src/control/control_math.c Application/src/control/controller.c
    Application/src/control/slew_controller.c Application/src/control/sun_pointing.c
    Application/src/estimation/attitude_estimator.c Application/src/estimation/kalman_filter.c
    Application/src/estimation/reference_vectors.c Application/src/guidance/target_generator.c)
target_include_directories(adcs_stm32 PRIVATE
    Core/Inc Platform/Inc Application/include
    ${HAL_DIR}/Inc ${HAL_DIR}/Inc/Legacy Drivers/CMSIS/Include
    Drivers/CMSIS/Device/ST/STM32G4xx/Include
    ${REPO_ROOT}/shared/interfaces ${REPO_ROOT}/shared/csp)
target_link_libraries(adcs_stm32 PRIVATE csp freertos_kernel m)
set_target_properties(adcs_stm32 PROPERTIES SUFFIX ".elf")
target_link_options(adcs_stm32 PRIVATE
    -T${CMAKE_CURRENT_SOURCE_DIR}/STM32G431RBTX_FLASH.ld
    --specs=nano.specs --specs=nosys.specs
    -Wl,--gc-sections -Wl,--print-memory-usage
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/adcs_stm32.map)
set_property(TARGET adcs_stm32 APPEND PROPERTY LINK_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/STM32G431RBTX_FLASH.ld)
add_custom_command(TARGET adcs_stm32 POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:adcs_stm32> ${CMAKE_CURRENT_BINARY_DIR}/adcs_stm32.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:adcs_stm32> ${CMAKE_CURRENT_BINARY_DIR}/adcs_stm32.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:adcs_stm32>)
