add_executable(obc_supervisor
    src/main.c
    src/supervisor.c
)

target_include_directories(obc_supervisor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
