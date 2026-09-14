add_executable(adcs_sim
    src/main.c
    src/freertos_hooks.c
    src/manager/fault_manager.c
    src/manager/adcs_manager.c
    src/tasks/command_task.c
    src/tasks/control_task.c
    src/tasks/estimation_task.c
    src/tasks/housekeeping_task.c
    src/tasks/sensor_task.c
    src/tasks/telemetry_task.c
    src/communication/command_handler.c
    src/communication/telemetry.c
    src/control/actuator_allocator.c
    src/control/bdot.c
    src/control/control_math.c
    src/control/controller.c
    src/control/slew_controller.c
    src/control/sun_pointing.c
    src/estimation/attitude_estimator.c
    src/estimation/kalman_filter.c
    src/estimation/reference_vectors.c
    src/guidance/target_generator.c
    src/simulation/adcs_simulator.c
)

target_include_directories(adcs_sim PRIVATE
    ${CMAKE_SOURCE_DIR}/rtos/kernels/include
    ${CMAKE_SOURCE_DIR}/rtos/ports/posix
    ${CMAKE_SOURCE_DIR}/rtos/config
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/libs/cglm/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/manager
)

target_link_libraries(adcs_sim PRIVATE freertos_kernel pthread platform csp_interface cglm m)

if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(adcs_sim PRIVATE -Wall -Wextra -Wpedantic)
endif()
