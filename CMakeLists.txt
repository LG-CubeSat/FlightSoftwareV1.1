cmake_minimum_required(VERSION 3.16)
project(FlightSoftwareV1 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(HW_MODE "Decide whether to build for hardware (ON) or simulation (OFF)" OFF)

if (HW_MODE)
    message(STATUS "Hardware Mode is Enabled.")
else()
    message(STATUS "Simulation Mode is Enabled.")
endif()

# ASan/UBSan for debugging SIM builds -- host-only instrumentation, doesn't
# apply to the cross-compiled HW_MODE target. Each FreeRTOS task in
# rtos/ports/posix/port.c is a real pthread (not a ucontext-swapped fiber),
# so this needs no fiber-registration workarounds to be accurate.
option(ENABLE_SANITIZERS "Build with AddressSanitizer + UndefinedBehaviorSanitizer (SIM only)" OFF)

# Separate from ENABLE_SANITIZERS on purpose. Verified by hand: ASan's
# `address` check replaces the malloc implementation with its own
# instrumented allocator, which macOS's `leaks` tool cannot introspect at
# all ("target process is using Address Sanitizer which doesn't work with
# memory analysis tools" / "unable to inspect heap ranges of target
# process"). UndefinedBehaviorSanitizer alone has no such conflict (also
# verified by hand) -- so a leak-check build on macOS gets UBSan but not
# ASan, while Linux's LeakSanitizer has no such restriction and can run
# either standalone or alongside full ASan.
option(ENABLE_LEAK_CHECK "Enable leak checking (SIM only) -- LeakSanitizer on Linux, macOS 'leaks' tool on macOS" OFF)

if (APPLE AND ENABLE_SANITIZERS AND ENABLE_LEAK_CHECK)
    message(FATAL_ERROR
        "ENABLE_SANITIZERS (AddressSanitizer) and ENABLE_LEAK_CHECK (macOS "
        "'leaks' tool) cannot be combined on macOS -- ASan's allocator "
        "isn't introspectable by 'leaks'. Use two separate build "
        "directories: one with -DENABLE_SANITIZERS=ON for memory/UB bugs, "
        "another with -DENABLE_LEAK_CHECK=ON (ENABLE_SANITIZERS left OFF) "
        "for leak checking.")
endif()

set(_sanitize_checks "")
if (NOT HW_MODE)
    if (ENABLE_SANITIZERS)
        list(APPEND _sanitize_checks address undefined)
    elseif (ENABLE_LEAK_CHECK)
        # Leak-check-only build (no full ASan, see comment above).
        list(APPEND _sanitize_checks undefined)
        if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
            list(APPEND _sanitize_checks leak) # LSan can run standalone on Linux
        endif()
    endif()
elseif (ENABLE_SANITIZERS OR ENABLE_LEAK_CHECK)
    message(WARNING "ENABLE_SANITIZERS/ENABLE_LEAK_CHECK ignored: not supported with HW_MODE")
endif()

if (_sanitize_checks)
    list(JOIN _sanitize_checks "," _sanitize_str)
    message(STATUS "Sanitizers enabled: ${_sanitize_str}")
    add_compile_options(-fsanitize=${_sanitize_str} -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=${_sanitize_str})
endif()

# added on
add_subdirectory(rtos)

# libcsp defaults to CSP_PORT_MAX_BIND=16, but docs/architecture.md's port
# scheme uses 10-19 for commands, 20-29 for telemetry, and 30 for the ground
# link -- raise the bindable range to cover it.
set(CSP_PORT_MAX_BIND 31 CACHE STRING "Maximum number of bindable ports")

add_subdirectory(libs/libcsp)

add_subdirectory(shared)
add_subdirectory(sys)
add_subdirectory(apps/adcs)
add_subdirectory(apps/obc)

# toggle for the platform code
add_subdirectory(platform)

option(BUILD_TESTS "Build simple sanity/integration tests" ON)

if (BUILD_TESTS AND NOT HW_MODE)
    enable_testing()
    add_subdirectory(tests)
endif()