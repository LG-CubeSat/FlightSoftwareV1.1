cmake_minimum_required(VERSION 3.10)
project(obc_simulator)

set(CMAKE_C_STANDARD 11)

# Source Files
set(SOURCES
    main.c
    adcs/adcs.c
    system/system.c
    communication/spi.c
    ../shared/ipc/v_bus.c
)

# Include Directories
include_directories(
    .
    adcs
    communication
    systen
    ../shared/ipc
    ../shared/protocol
)

# Executable
add_executable(obc_sim ${SOURCES})

# Link Libraries
target_link_libraries(obc_sim pthread)
