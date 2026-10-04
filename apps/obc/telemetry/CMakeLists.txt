add_library(obc_telemetry STATIC
    src/obc_telemetry_protocol.c
)

target_include_directories(obc_telemetry
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(obc_telemetry
    PUBLIC
        obc_ipc
)