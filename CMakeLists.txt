cmake_minimum_required(VERSION 3.16)
project(FlightSoftwareV1 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(rtos)
add_subdirectory(platform)
add_subdirectory(drivers)
add_subdirectory(shared)
add_subdirectory(sys)
add_subdirectory(apps/adcs)
add_subdirectory(apps/obc)