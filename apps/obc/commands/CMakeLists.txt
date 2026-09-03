add_executable(obc_commands
    src/main.c
    src/ingest.c
    src/relay.c
)

target_include_directories(obc_commands PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(obc_commands PRIVATE obc_ipc csp_interface pthread)
