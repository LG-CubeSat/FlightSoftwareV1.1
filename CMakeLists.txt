cmake_minimum_required(VERSION 3.16)
project(FlightSoftwareV1 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

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

add_subdirectory(shared)
add_subdirectory(sys)
add_subdirectory(apps/adcs)

# so supervisor can launch all the programs
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
add_subdirectory(apps/obc)

# toggle for the platform code
add_subdirectory(platform)

option(BUILD_TESTS "Build simple sanity/integration tests" ON)

if (BUILD_TESTS AND NOT HW_MODE)
    enable_testing()
    add_subdirectory(tests)
endif()