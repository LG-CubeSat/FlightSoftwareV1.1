add_executable(obc_compute
    src/main.c
    src/image_compression.c
)

target_include_directories(obc_compute PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(obc_compute PRIVATE obc_ipc pthread)
