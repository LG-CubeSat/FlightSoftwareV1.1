add_executable(eps_sim
    src/main.c
    src/freertos_hooks.c
    src/manager/eps_manager.c
    src/manager/fault_manager.c
    src/communication/command_handler.c
    src/communication/telemetry.c
    src/drivers/sim_eps_drivers.c
    src/simulation/eps_simulator.c
    src/tasks/command_task.c
    src/tasks/estimation_task.c
    src/tasks/housekeeping_task.c
    src/tasks/sensor_task.c
    src/tasks/telemetry_task.c
    src/utils/conversions.c
    src/utils/power_calculations.c
)

target_include_directories(eps_sim PRIVATE
    ${CMAKE_SOURCE_DIR}/rtos/kernels/include
    ${CMAKE_SOURCE_DIR}/rtos/ports/posix
    ${CMAKE_SOURCE_DIR}/rtos/config
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(eps_sim PRIVATE freertos_kernel pthread platform csp_interface m)

if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(eps_sim PRIVATE -Wall -Wextra -Wpedantic)
endif()
