add_library(obc_ipc STATIC
    src/obc_ipc.c
)

target_include_directories(obc_ipc
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

