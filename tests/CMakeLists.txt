add_executable(v_bus_test test_v_bus.c)
target_link_libraries(v_bus_test PRIVATE platform)

add_test(NAME v_bus_test COMMAND v_bus_test)
set_tests_properties(v_bus_test PROPERTIES TIMEOUT 10)
