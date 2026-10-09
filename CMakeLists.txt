cmake_minimum_required(VERSION 3.16)
project(FlightSoftwareV1 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Emit compile_commands.json so editor tooling (clangd / the VS Code C/C++
# extension) resolves the exact per-target include paths and defines the
# compiler uses. Without it, identically-named headers across subsystems
# (e.g. communication/message.h in both apps/adcs and apps/eps) can resolve to
# the wrong copy in IntelliSense.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(HW_MODE "Decide whether to build for hardware (ON) or simulation (OFF)" OFF)

if (HW_MODE)
    message(STATUS "Hardware Mode is Enabled.")
else()
    message(STATUS "Simulation Mode is Enabled.")
endif()

# added on
add_subdirectory(rtos)

# libcsp defaults to CSP_PORT_MAX_BIND=16, but docs/architecture.md's port
# scheme uses 10-19 for commands, 20-29 for telemetry, and 30 for the ground
# link -- raise the bindable range to cover it.
set(CSP_PORT_MAX_BIND 31 CACHE STRING "Maximum number of bindable ports")

add_subdirectory(libs/libcsp)

# ssdv doesn't ship its own CMake project (just a plain Makefile), so it
# isn't an add_subdirectory candidate the way libcsp is -- it's declared
# directly here instead of adding a CMakeLists.txt inside libs/ssdv/,
# which would mix our build config into a vendored third-party submodule.
add_library(ssdv STATIC
    libs/ssdv/ssdv.c
    libs/ssdv/rs8.c
)
target_include_directories(ssdv PUBLIC libs/ssdv)

add_subdirectory(shared)
add_subdirectory(sys)

# ADCS/EPS target an STM32 running FreeRTOS. On HW_MODE=ON they'd currently
# link the Linux-only real I2C driver and the POSIX FreeRTOS port -- neither
# of which is correct for real silicon -- so they're SIM-only until the ARM
if (NOT HW_MODE)
    add_subdirectory(apps/adcs)
    add_subdirectory(apps/eps)
endif()

# so supervisor can launch all the programs
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
add_subdirectory(apps/obc)

# toggle for the platform code
add_subdirectory(platform)

option(BUILD_TESTS "Build simple sanity/integration tests" ON)

if (BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()