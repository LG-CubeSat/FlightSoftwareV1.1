# Leak checking, picked per-OS. Driven by ENABLE_LEAK_CHECK specifically,
# not ENABLE_SANITIZERS -- the two are mutually exclusive on macOS (see the
# top-level CMakeLists.txt comment: ASan's allocator isn't introspectable by
# `leaks`), so this needs to be independently selectable.
#
# Full writeup, including everything verified below: docs/sanitizers_and_leak_checking.md
#
#   Linux:   LeakSanitizer runs automatically once compiled in (top-level
#            CMakeLists.txt adds -fsanitize=leak, alongside address+undefined
#            if ENABLE_SANITIZERS is also on, or standalone if not) -- just
#            make that explicit via ASAN_OPTIONS rather than relying on
#            whatever the local default happens to be. It's linked-in
#            instrumentation inside the target binary itself, so it doesn't
#            have the macOS problem described below.
#   macOS:   LeakSanitizer itself is not implemented on this platform
#            (verified: `clang -fsanitize=leak` -> "unsupported option ...
#            for target 'arm64-apple-darwin'"). `leaks --atExit` is the
#            native substitute, BUT IT IS NOT WIRED INTO THESE FOUR TESTS --
#            verified by hand that it actively corrupts them: every one of
#            comms_bus_test, comms_bus_addressing_test, position_command_test,
#            and command_ack_test forks subprocesses and checks their exit
#            status via WIFEXITED/WEXITSTATUS. Running any of them under
#            `leaks --atExit` (as an external process that becomes the
#            binary's direct parent via task-port attachment) corrupts that
#            exit-status reporting -- comms_bus_test's own sub-checks printed
#            PASS while its fork-status aggregation reported FAIL, purely as
#            an artifact of being leaks-wrapped (confirmed: passes plainly,
#            passes with just MallocStackLogging=1 set, only breaks once
#            `leaks --atExit` is the actual parent). Worse, `leaks --atExit`'s
#            own exit code reflects only its leak result, not the wrapped
#            binary's real exit code -- so wrapping would have silently
#            reported that corrupted FAIL as a passing CTest run. Given both
#            problems compound, wrapping these specific four tests would be
#            worse than no leak-checking at all, so it's left undone for
#            them. obc_sim/adcs_sim themselves don't fork (pthreads, not
#            fork -- see rtos/ports/posix/port.c), so they don't hit this
#            problem; sim_leak_check_macos.sh checks them directly, live,
#            registered below as sim_leak_check_macos.
#   Windows: no built-in equivalent wired up here (Dr. Memory / Application
#            Verifier would be the tools to reach for) -- this codebase's
#            SIM backend uses POSIX sockets/pthreads directly anyway, so it
#            doesn't build natively on Windows outside WSL (which CMake
#            reports as Linux, and is covered by the Linux branch above).
if (ENABLE_LEAK_CHECK AND NOT HW_MODE)
    if (APPLE)
        message(STATUS "Leak checking: comms_bus_test/comms_bus_addressing_test/position_command_test/command_ack_test skipped (all four fork; see comment above). obc_sim/adcs_sim checked directly via sim_leak_check_macos.")
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(STATUS "Leak checking: LeakSanitizer (forced on via ASAN_OPTIONS)")
    elseif (WIN32)
        message(WARNING "Leak checking not available on Windows: no LeakSanitizer/leaks equivalent wired up here.")
    endif()
endif()

function(add_leak_checked_test test_name timeout_sec)
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES TIMEOUT ${timeout_sec})

    if (NOT APPLE AND ENABLE_LEAK_CHECK AND NOT HW_MODE AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set_tests_properties(${test_name} PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=1")
    endif()
endfunction()

add_executable(comms_bus_test test_comms_bus.c)
target_link_libraries(comms_bus_test PRIVATE platform)
add_leak_checked_test(comms_bus_test 10)

add_executable(comms_bus_addressing_test test_comms_bus_addressing.c)
target_link_libraries(comms_bus_addressing_test PRIVATE platform)
add_leak_checked_test(comms_bus_addressing_test 10)

add_executable(position_command_test test_position_command.c)
target_compile_definitions(position_command_test PRIVATE
    OBC_SIM_PATH="$<TARGET_FILE:obc_sim>"
    ADCS_SIM_PATH="$<TARGET_FILE:adcs_sim>"
)
add_dependencies(position_command_test obc_sim adcs_sim)
add_leak_checked_test(position_command_test 30)

add_executable(command_ack_test test_command_ack.c)
target_compile_definitions(command_ack_test PRIVATE
    OBC_SIM_PATH="$<TARGET_FILE:obc_sim>"
    ADCS_SIM_PATH="$<TARGET_FILE:adcs_sim>"
)
add_dependencies(command_ack_test obc_sim adcs_sim)
add_leak_checked_test(command_ack_test 30)

# obc_sim/adcs_sim leak-checked directly and live (see the big comment
# above for why this is separate from add_leak_checked_test). macOS only --
# on Linux, these same two binaries already get real LeakSanitizer coverage
# for free: position_command_test/command_ack_test fork+exec them with
# ASAN_OPTIONS=detect_leaks=1 in the inherited environment.
if (APPLE AND ENABLE_LEAK_CHECK AND NOT HW_MODE)
    add_test(NAME sim_leak_check_macos
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/sim_leak_check_macos.sh
                $<TARGET_FILE:obc_sim> $<TARGET_FILE:adcs_sim>)
    set_tests_properties(sim_leak_check_macos PROPERTIES TIMEOUT 30)
endif()
