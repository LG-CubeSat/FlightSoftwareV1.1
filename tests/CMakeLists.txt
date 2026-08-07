add_executable(v_bus_test test_v_bus.c)
target_link_libraries(v_bus_test PRIVATE platform)

add_test(NAME v_bus_test COMMAND v_bus_test)
set_tests_properties(v_bus_test PROPERTIES TIMEOUT 10)

add_executable(position_command_test test_position_command.c)
target_compile_definitions(position_command_test PRIVATE
    OBC_SIM_PATH="$<TARGET_FILE:obc_sim>"
    ADCS_SIM_PATH="$<TARGET_FILE:adcs_sim>"
)
add_dependencies(position_command_test obc_sim adcs_sim)

add_test(NAME position_command_test COMMAND position_command_test)
set_tests_properties(position_command_test PROPERTIES TIMEOUT 30)
