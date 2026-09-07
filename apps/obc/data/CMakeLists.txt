add_executable(obc_data
    src/main.c
    src/storage.c
    src/filesystem.c
    src/heartbeat.c
)

target_include_directories(obc_data PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(obc_data PRIVATE obc_ipc pthread)
