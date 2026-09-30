add_library(obc_ssdv_codec STATIC
    src/ssdv_codec.c
)

target_include_directories(obc_ssdv_codec PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(obc_ssdv_codec PUBLIC ssdv)

add_executable(obc_compute
    src/main.c
    src/dispatch.c
    src/worker.c
    src/heartbeat.c
    src/compute_health.c
)

target_include_directories(obc_compute PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(obc_compute PRIVATE obc_ipc pthread obc_ssdv_codec obc_progress)