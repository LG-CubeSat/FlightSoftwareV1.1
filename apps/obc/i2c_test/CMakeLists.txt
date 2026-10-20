add_executable(obc_adcs_i2c_test
    src/main.c
)

target_include_directories(obc_adcs_i2c_test PRIVATE
    ${CMAKE_SOURCE_DIR}/platform/real/include
    ${CMAKE_SOURCE_DIR}/shared/interfaces
)

target_link_libraries(obc_adcs_i2c_test PRIVATE
    frame_codec
    i2c_protocol
)
