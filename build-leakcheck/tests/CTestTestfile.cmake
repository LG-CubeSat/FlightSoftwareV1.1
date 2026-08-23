# CMake generated Testfile for 
# Source directory: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1/tests
# Build directory: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1/build-leakcheck/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(comms_bus_test "/Users/wesleywong/Documents/GitHub/FlightSoftwareV1/build-leakcheck/tests/comms_bus_test")
set_tests_properties(comms_bus_test PROPERTIES  TIMEOUT "10" _BACKTRACE_TRIPLES "/Users/wesleywong/Documents/GitHub/FlightSoftwareV1/tests/CMakeLists.txt;55;add_test;/Users/wesleywong/Documents/GitHub/FlightSoftwareV1/tests/CMakeLists.txt;65;add_leak_checked_test;/Users/wesleywong/Documents/GitHub/FlightSoftwareV1/tests/CMakeLists.txt;0;")
add_test(comms_bus_addressing_test "/Users/wesleywong/Documents/GitHub/FlightSoftwareV1/build-leakcheck/tests/comms_bus_addressing_test")
set_tests_properties(comms_bus_addressing_test PROPERTIES  TIMEOUT "10" _BACKTRACE_TRIPLES "/Users/wesleywong/Documents/GitHub/FlightSoftwareV1/tests/CMakeLists.txt;55;add_test;/Users/wesleywong/Documents/GitHub/FlightSoftwareV1/tests/CMakeLists.txt;69;add_leak_checked_test;/Users/wesleywong/Documents/GitHub/FlightSoftwareV1/tests/CMakeLists.txt;0;")
add_test(position_command_test "/Users/wesleywong/Documents/GitHub/FlightSoftwareV1/build-leakcheck/tests/position_command_test")
set_tests_properties(position_command_test PROPERTIES  TIMEOUT "30" _BACKTRACE_TRIPLES "/Users/wesleywong/Documents/GitHub/FlightSoftwareV1/tests/CMakeLists.txt;55;add_test;/Users/wesleywong/Documents/GitHub/FlightSoftwareV1/tests/CMakeLists.txt;77;add_leak_checked_test;/Users/wesleywong/Documents/GitHub/FlightSoftwareV1/tests/CMakeLists.txt;0;")
add_test(command_ack_test "/Users/wesleywong/Documents/GitHub/FlightSoftwareV1/build-leakcheck/tests/command_ack_test")
set_tests_properties(command_ack_test PROPERTIES  TIMEOUT "30" _BACKTRACE_TRIPLES "/Users/wesleywong/Documents/GitHub/FlightSoftwareV1/tests/CMakeLists.txt;55;add_test;/Users/wesleywong/Documents/GitHub/FlightSoftwareV1/tests/CMakeLists.txt;85;add_leak_checked_test;/Users/wesleywong/Documents/GitHub/FlightSoftwareV1/tests/CMakeLists.txt;0;")
add_test(sim_leak_check_macos "/Users/wesleywong/Documents/GitHub/FlightSoftwareV1/tests/sim_leak_check_macos.sh" "/Users/wesleywong/Documents/GitHub/FlightSoftwareV1/build-leakcheck/apps/obc/obc_sim" "/Users/wesleywong/Documents/GitHub/FlightSoftwareV1/build-leakcheck/apps/adcs/adcs_sim")
set_tests_properties(sim_leak_check_macos PROPERTIES  TIMEOUT "30" _BACKTRACE_TRIPLES "/Users/wesleywong/Documents/GitHub/FlightSoftwareV1/tests/CMakeLists.txt;93;add_test;/Users/wesleywong/Documents/GitHub/FlightSoftwareV1/tests/CMakeLists.txt;0;")
