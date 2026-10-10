add_library(frame_codec STATIC
    interfaces/frame.c
)

target_include_directories(frame_codec
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/interfaces
)

add_library(csp_interface STATIC
    csp/csp_if_comms_bus.c
    csp/csp_network.c
)

target_include_directories(csp_interface
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/csp
)

target_link_libraries(csp_interface
    PUBLIC
        csp
        platform
)

add_library(obc_logging STATIC
    logging/obc_log.c
)

target_include_directories(obc_logging PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/logging
)

target_link_libraries(obc_logging PUBLIC
    pthread
)

add_library(i2c_protocol STATIC
    interfaces/i2c_protocol.c
)

target_include_directories(i2c_protocol PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/interfaces
)