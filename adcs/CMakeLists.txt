cmake_minimum_required(VERSION 3.10)
project(adcs_simulator)

# 1. Compiler Flags for Native Simulation
set(CMAKE_C_STANDARD 11)
add_definitions(-D_XOPEN_SOURCE=700) # Required for some POSIX functions

# 2. Source Files
set(SOURCES
    src/main.c
    src/communication/packet_parser.c
    src/communication/spi.c
    src/commands/command_handler.c
    src/commands/command_queue.c
    src/commands/command_task.c
    src/control/control_task.c
    src/control/control_queue.c
    src/control/adcs_context.c
    ../shared/protocol/packet.c
    
    # FreeRTOS Kernel
    FreeRTOS/source/tasks.c
    FreeRTOS/source/queue.c
    FreeRTOS/source/list.c
    FreeRTOS/source/timers.c
    FreeRTOS/source/event_groups.c
    FreeRTOS/MemMang/heap_4.c
    
    # FreeRTOS POSIX Port (We will create these next)
    FreeRTOS/port.c
)

# 3. Include Directories
include_directories(
    src
    src/communication
    src/commands
    src/control
    ../shared/protocol
    ../include
    FreeRTOS/include
    FreeRTOS
)

# 4. Executable
add_executable(adcs_sim ${SOURCES})

# 5. Link Libraries (POSIX Threads for Simulation)
target_link_libraries(adcs_sim pthread)
