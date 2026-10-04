add_executable(obc_data
    src/main.c
    src/storage.c
    src/filesystem.c
    src/heartbeat.c
    src/data_health.c
    src/telemetry_store.c
)

target_include_directories(obc_data PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(obc_data PRIVATE obc_ipc pthread obc_progress obc_telemetry)

if (HW_MODE)
    target_compile_definitions(
        obc_data
        PRIVATE
            DEFAULT_TELEMETRY_LOG_PATH="/var/lib/obc/telemetry.bin"
    )
else()
    target_compile_definitions(
        obc_data
        PRIVATE
            DEFAULT_TELEMETRY_LOG_PATH="/tmp/obc_telemetry.bin"
    )
endif()