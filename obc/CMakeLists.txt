cmake_minimum_required(VERSION 3.10)

project(main)

add_executable(main 
    main.c
    adcs/adcs.c
    communication/spi.c 
    system/system.c
    
    ${CMAKE_SOURCE_DIR}/../shared/protocol/packet.c
)