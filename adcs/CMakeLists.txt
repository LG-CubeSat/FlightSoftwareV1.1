cmake_minimum_required(VERSION 3.10)

project(main)

add_executable(main 
    src/main.c
    src/communication/packet_parser.c
    src/communication/spi.c
    src/commands/command_handler.c
    src/commands/command_queue.c
    src/commands/command_task.c
    
    ${CMAKE_SOURCE_DIR}/../shared/protocol/packet.c
)