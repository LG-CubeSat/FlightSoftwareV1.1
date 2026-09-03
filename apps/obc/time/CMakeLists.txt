add_executable(obc_time
    src/main.c
    src/time_sync.c
)

target_include_directories(obc_time PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(obc_time PRIVATE obc_ipc pthread)
