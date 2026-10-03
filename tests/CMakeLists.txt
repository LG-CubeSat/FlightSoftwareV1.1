if (NOT HW_MODE)
    add_executable(comms_bus_test test_comms_bus.c)
    target_link_libraries(comms_bus_test PRIVATE platform)

    add_test(NAME comms_bus_test COMMAND comms_bus_test)
    set_tests_properties(comms_bus_test PROPERTIES
        TIMEOUT 10
        RESOURCE_LOCK comms_bus_socket
    )

    add_executable(comms_bus_addressing_test test_comms_bus_addressing.c)
    target_link_libraries(comms_bus_addressing_test PRIVATE platform)

    add_test(NAME comms_bus_addressing_test COMMAND comms_bus_addressing_test)
    set_tests_properties(comms_bus_addressing_test PROPERTIES
        TIMEOUT 10
        RESOURCE_LOCK comms_bus_socket
    )

    add_executable(position_command_test test_position_command.c)
    target_link_libraries(position_command_test PRIVATE csp_interface pthread)
    target_compile_definitions(position_command_test PRIVATE
        ADCS_SIM_PATH="$<TARGET_FILE:adcs_sim>"
    )
    add_dependencies(position_command_test adcs_sim)

    add_test(NAME position_command_test COMMAND position_command_test)
    set_tests_properties(position_command_test PROPERTIES
        TIMEOUT 30
        RESOURCE_LOCK comms_bus_socket
    )

    add_executable(command_ack_test test_command_ack.c)
    target_link_libraries(command_ack_test PRIVATE csp_interface pthread)
    target_compile_definitions(command_ack_test PRIVATE
        ADCS_SIM_PATH="$<TARGET_FILE:adcs_sim>"
    )
    add_dependencies(command_ack_test adcs_sim)

    add_test(NAME command_ack_test COMMAND command_ack_test)
    set_tests_properties(command_ack_test PROPERTIES
        TIMEOUT 30
        RESOURCE_LOCK comms_bus_socket
    )

    add_executable(full_constellation_test test_full_constellation.c)
    target_compile_definitions(full_constellation_test PRIVATE
        OBC_SUPERVISOR_PATH="$<TARGET_FILE:obc_supervisor>"
        ADCS_SIM_PATH="$<TARGET_FILE:adcs_sim>"
    )
    add_dependencies(full_constellation_test obc_supervisor adcs_sim)

    add_test(NAME full_constellation_test COMMAND full_constellation_test)
    set_tests_properties(full_constellation_test PROPERTIES
        TIMEOUT 30
        RESOURCE_LOCK "comms_bus_socket;obc_ipc_sockets"
    )
endif()

# These tests exercise code shared by SIM and the Raspberry Pi build. They
# deliberately do not link `platform`, so HW_MODE does not pull in a real
# camera, radio, or Linux I2C device just to test portable logic.
add_executable(frame_codec_test test_frame_codec.c)
target_link_libraries(frame_codec_test PRIVATE frame_codec)
add_test(NAME frame_codec_test COMMAND frame_codec_test)
set_tests_properties(frame_codec_test PROPERTIES TIMEOUT 5)

add_executable(obc_ipc_test test_obc_ipc.c)
target_link_libraries(obc_ipc_test PRIVATE obc_ipc)
add_test(NAME obc_ipc_test COMMAND obc_ipc_test)
set_tests_properties(obc_ipc_test PROPERTIES
    TIMEOUT 5
    RESOURCE_LOCK obc_ipc_sockets
)

add_executable(ssdv_roundtrip_test test_ssdv_roundtrip.c)
target_link_libraries(ssdv_roundtrip_test PRIVATE obc_ssdv_codec)
add_test(NAME ssdv_roundtrip_test COMMAND ssdv_roundtrip_test)
set_tests_properties(ssdv_roundtrip_test PROPERTIES TIMEOUT 5)

add_executable(compute_async_test test_compute_async.c)
target_link_libraries(compute_async_test PRIVATE obc_ipc)
target_include_directories(compute_async_test PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/obc/include
)
target_compile_definitions(compute_async_test PRIVATE
    DATA_PATH="$<TARGET_FILE:obc_data>"
    COMPUTE_PATH="$<TARGET_FILE:obc_compute>"
)
add_dependencies(compute_async_test obc_data obc_compute)

add_test(NAME compute_async_test COMMAND compute_async_test)
set_tests_properties(compute_async_test PROPERTIES
    TIMEOUT 30
    RESOURCE_LOCK obc_ipc_sockets
)

add_executable(compute_progress_test test_compute_progress.c)
target_link_libraries(compute_progress_test PRIVATE obc_ipc)
target_include_directories(compute_progress_test PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/obc/include
)
target_compile_definitions(compute_progress_test PRIVATE
    COMPUTE_PATH="$<TARGET_FILE:obc_compute>"
)
add_dependencies(compute_progress_test obc_compute)

add_test(NAME compute_progress_test COMMAND compute_progress_test)
set_tests_properties(compute_progress_test PROPERTIES
    TIMEOUT 20
    RESOURCE_LOCK obc_ipc_sockets
)

add_executable(obc_progress_test test_obc_progress.c)
target_link_libraries(obc_progress_test PRIVATE obc_progress)
add_test(NAME obc_progress_test COMMAND obc_progress_test)
set_tests_properties(obc_progress_test PROPERTIES TIMEOUT 5)
