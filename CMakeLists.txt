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

add_subdirectory(rtos)
add_subdirectory(drivers)
add_subdirectory(shared)
add_subdirectory(sys)
add_subdirectory(apps/adcs)
add_subdirectory(apps/obc)

# toggle for the platform code
add_subdirectory(platform)

option(BUILD_TESTS "Build simple sanity/integration tests" ON)

if (BUILD_TESTS AND NOT HW_MODE)
    enable_testing()
    add_subdirectory(tests)
endif()