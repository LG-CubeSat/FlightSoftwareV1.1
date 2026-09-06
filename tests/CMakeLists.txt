add_executable(comms_bus_test test_comms_bus.c)
target_link_libraries(comms_bus_test PRIVATE platform)

add_test(NAME comms_bus_test COMMAND comms_bus_test)
set_tests_properties(comms_bus_test PROPERTIES TIMEOUT 10)

add_executable(comms_bus_addressing_test test_comms_bus_addressing.c)
target_link_libraries(comms_bus_addressing_test PRIVATE platform)

add_test(NAME comms_bus_addressing_test COMMAND comms_bus_addressing_test)
set_tests_properties(comms_bus_addressing_test PROPERTIES TIMEOUT 10)

add_executable(position_command_test test_position_command.c)
target_link_libraries(position_command_test PRIVATE csp_interface pthread)
target_compile_definitions(position_command_test PRIVATE
    ADCS_SIM_PATH="$<TARGET_FILE:adcs_sim>"
)
add_dependencies(position_command_test adcs_sim)

add_test(NAME position_command_test COMMAND position_command_test)
set_tests_properties(position_command_test PROPERTIES TIMEOUT 30)

add_executable(command_ack_test test_command_ack.c)
target_link_libraries(command_ack_test PRIVATE csp_interface pthread)
target_compile_definitions(command_ack_test PRIVATE
    ADCS_SIM_PATH="$<TARGET_FILE:adcs_sim>"
)
add_dependencies(command_ack_test adcs_sim)

add_test(NAME command_ack_test COMMAND command_ack_test)
set_tests_properties(command_ack_test PROPERTIES TIMEOUT 30)
