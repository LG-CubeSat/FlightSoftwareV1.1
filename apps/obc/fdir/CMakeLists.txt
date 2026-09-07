add_executable(obc_fdir
    src/main.c
    src/health_monitor.c
    src/watchdog.c
    src/fallback.c
)

target_include_directories(obc_fdir PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(obc_fdir PRIVATE obc_ipc csp_interface pthread)
