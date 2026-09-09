add_executable(obc_compute
    src/main.c
    src/bitstream.c
    src/rice_codec.c
    src/compute.c
    src/heartbeat.c
)

target_include_directories(obc_compute PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(obc_compute PRIVATE obc_ipc pthread)
