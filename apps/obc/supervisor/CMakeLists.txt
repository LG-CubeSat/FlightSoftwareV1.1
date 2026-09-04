add_executable(obc_supervisor
    src/main.c
    src/supervisor.c
    src/processes.c
)

target_include_directories(obc_supervisor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(obc_supervisor PRIVATE obc_ipc pthread)
